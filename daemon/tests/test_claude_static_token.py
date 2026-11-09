"""Long-lived Claude token (`claude setup-token`) handling.

The Claude Code login's refresh token was repeatedly rejected (HTTP 400) after
the PC had been off for a few days, so the display showed nothing for Claude
until a manual /login. These tests pin the replacement: a static token wins
over the login, and a rejected static token falls back to the login instead of
leaving the display blank.
"""

import asyncio
import json
import sys
from pathlib import Path

import httpx
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from providers import claude  # noqa: E402
from providers.claude import ClaudeProvider  # noqa: E402

HEADERS = {
    "anthropic-ratelimit-unified-5h-utilization": "0.42",
    "anthropic-ratelimit-unified-5h-reset": "0",
    "anthropic-ratelimit-unified-7d-utilization": "0.17",
    "anthropic-ratelimit-unified-7d-reset": "0",
    "anthropic-ratelimit-unified-5h-status": "allowed",
}


@pytest.fixture
def env(tmp_path, monkeypatch):
    token_file = tmp_path / "claude-token"
    creds_file = tmp_path / ".credentials.json"
    monkeypatch.setattr(claude, "TOKEN_FILE_PATH", token_file)
    monkeypatch.setattr(claude, "CREDENTIALS_PATH", creds_file)
    monkeypatch.setattr(claude.sys, "platform", "win32")
    monkeypatch.delenv(claude.TOKEN_ENV_VAR, raising=False)
    logs: list[str] = []
    return token_file, creds_file, ClaudeProvider(logs.append), logs


def _write_login(creds_file: Path, access="login-access", refresh="login-refresh"):
    creds_file.write_text(
        json.dumps({"claudeAiOauth": {"accessToken": access, "refreshToken": refresh}})
    )


def _mock_api(monkeypatch, valid_tokens: set[str]):
    seen: list[str] = []
    real = httpx.AsyncClient

    def handler(request: httpx.Request) -> httpx.Response:
        if request.url.path.endswith("/oauth/token"):
            return httpx.Response(400)
        token = request.headers["Authorization"].removeprefix("Bearer ")
        seen.append(token)
        if token in valid_tokens:
            return httpx.Response(200, headers=HEADERS, json={})
        return httpx.Response(401)

    monkeypatch.setattr(
        claude.httpx,
        "AsyncClient",
        lambda **kw: real(transport=httpx.MockTransport(handler), **kw),
    )
    return seen


def test_token_file_wins_over_login(env):
    token_file, creds_file, provider, _ = env
    _write_login(creds_file)
    token_file.write_text("﻿sk-ant-oat01-abc\r\n", encoding="utf-8")
    creds = provider.read_credentials()
    assert creds.access_token == "sk-ant-oat01-abc"
    assert creds.storage == "static-file"
    assert creds.refresh_token is None


def test_env_var_wins_over_file(env, monkeypatch):
    token_file, _, provider, _ = env
    token_file.write_text("sk-ant-oat01-file")
    monkeypatch.setenv(claude.TOKEN_ENV_VAR, "sk-ant-oat01-env")
    assert provider.read_credentials().access_token == "sk-ant-oat01-env"


def test_no_token_file_uses_login(env):
    token_file, creds_file, provider, logs = env
    _write_login(creds_file)
    assert provider.read_credentials().access_token == "login-access"
    assert logs == ["Claude auth: Claude Code login"]


def test_empty_token_file_uses_login(env):
    token_file, creds_file, provider, _ = env
    _write_login(creds_file)
    token_file.write_text("  \n")
    assert provider.read_credentials().access_token == "login-access"


def test_static_token_polls_without_refresh(env, monkeypatch):
    token_file, _, provider, logs = env
    token_file.write_text("sk-ant-oat01-good")
    seen = _mock_api(monkeypatch, {"sk-ant-oat01-good"})
    usage = asyncio.run(provider.fetch_usage())
    assert usage.ok and usage.session_pct == 42 and usage.weekly_pct == 17
    assert seen == ["sk-ant-oat01-good"]
    assert not any("refresh" in m for m in logs)


def test_rejected_static_token_falls_back_to_login(env, monkeypatch):
    token_file, creds_file, provider, logs = env
    token_file.write_text("sk-ant-oat01-revoked")
    _write_login(creds_file)
    seen = _mock_api(monkeypatch, {"login-access"})

    usage = asyncio.run(provider.fetch_usage())
    assert usage.ok
    assert seen == ["sk-ant-oat01-revoked", "login-access"]
    assert any("Long-lived Claude token rejected" in m for m in logs)

    # Parked: the next poll goes straight to the login, no repeated 401 noise.
    seen.clear()
    asyncio.run(provider.fetch_usage())
    assert seen == ["login-access"]


def test_replacing_rejected_token_recovers(env, monkeypatch):
    token_file, creds_file, provider, _ = env
    token_file.write_text("sk-ant-oat01-revoked")
    _write_login(creds_file)
    seen = _mock_api(monkeypatch, {"login-access", "sk-ant-oat01-new"})
    asyncio.run(provider.fetch_usage())

    token_file.write_text("sk-ant-oat01-new")
    seen.clear()
    asyncio.run(provider.fetch_usage())
    assert seen == ["sk-ant-oat01-new"]


def test_login_refresh_path_unchanged(env, monkeypatch):
    """Without a static token the old 401 -> refresh flow still runs."""
    _, creds_file, provider, logs = env
    _write_login(creds_file, access="expired")
    _mock_api(monkeypatch, set())
    assert asyncio.run(provider.fetch_usage()) is None
    assert "API HTTP 401; attempting OAuth refresh" in logs
    assert "OAuth refresh HTTP 400" in logs


def test_old_token_file_warns_once(env, monkeypatch):
    token_file, _, provider, logs = env
    token_file.write_text("sk-ant-oat01-old")
    old = claude.time.time() - 340 * 86400
    import os

    os.utime(token_file, (old, old))
    provider.read_credentials()
    provider.read_credentials()
    assert sum("days old" in m for m in logs) == 1


def test_empty_login_token_is_treated_as_missing(env):
    """Seen 09.11.2026 while setup-token ran: login file held an empty token."""
    _, creds_file, provider, logs = env
    _write_login(creds_file, access="")
    assert provider.read_credentials() is None
    assert "Claude Code login has an empty access token" in logs
