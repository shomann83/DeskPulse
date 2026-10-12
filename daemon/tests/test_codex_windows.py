"""Rate-limit window classification for the ChatGPT / Codex provider.

The July 2026 removal of the 5h window broke silently: primary_window started
carrying the weekly limit while the provider kept reading it as the session
window, so the device showed a weekly percentage under a "5h" label. These
tests pin the length-based classification that replaced the positional one.
"""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from providers.codex import CodexProvider  # noqa: E402


@pytest.fixture
def provider():
    return CodexProvider(lambda msg: None)


def _payload(rate_limit, **extra):
    return {"plan_type": "plus", "rate_limit": rate_limit, **extra}


def test_legacy_dual_windows(provider):
    """Pre-July-2026 shape: 5h primary + 7d secondary."""
    usage = provider._usage_from_payload(
        _payload(
            {
                "allowed": True,
                "limit_reached": False,
                "primary_window": {
                    "used_percent": 42,
                    "limit_window_seconds": 18000,
                    "reset_after_seconds": 3600,
                },
                "secondary_window": {
                    "used_percent": 13,
                    "limit_window_seconds": 604800,
                    "reset_after_seconds": 172800,
                },
            }
        )
    )

    assert usage.ok is True
    assert usage.status == "allowed"
    assert usage.session_pct == 42
    assert usage.session_window_s == 18000
    assert usage.session_reset_min == 60
    assert usage.weekly_pct == 13
    assert usage.weekly_window_s == 604800
    assert usage.weekly_reset_min == 2880


def test_weekly_only_live_payload(provider):
    """Current shape, captured live from a Plus account on 2026-07-27."""
    usage = provider._usage_from_payload(
        _payload(
            {
                "allowed": True,
                "limit_reached": False,
                "primary_window": {
                    "used_percent": 4,
                    "limit_window_seconds": 604800,
                    "reset_after_seconds": 596865,
                },
                "secondary_window": None,
            },
            credits={
                "has_credits": False,
                "unlimited": False,
                "balance": "0",
                "approx_local_messages": [0, 0],
            },
            rate_limit_reached_type=None,
        )
    )

    # The weekly figure must land in the weekly slot, not under "5h".
    assert usage.weekly_pct == 4
    assert usage.weekly_window_s == 604800
    assert usage.weekly_reset_min == round(596865 / 60)
    # No session window exists — absent, not zero-used.
    assert usage.session_window_s == 0
    assert usage.session_pct == 0
    assert usage.session_reset_min == 0
    # Credits fill the freed slot on the device.
    assert usage.credits_valid is True
    assert usage.credits_has is False
    assert usage.credits_balance == "0.00"
    assert usage.credits_msgs == 0


def test_windows_without_declared_length_fall_back_to_position(provider):
    """No limit_window_seconds anywhere — keep the historical pairing."""
    usage = provider._usage_from_payload(
        _payload(
            {
                "allowed": True,
                "primary_window": {"used_percent": 30, "reset_after_seconds": 600},
                "secondary_window": {"used_percent": 7, "reset_after_seconds": 6000},
            }
        )
    )

    assert usage.session_pct == 30
    assert usage.session_window_s == 5 * 3600
    assert usage.weekly_pct == 7
    assert usage.weekly_window_s == 7 * 86400


def test_session_window_returning_reclaims_its_slot(provider):
    """If OpenAI reinstates a 5h window it takes the session slot back.

    Order is deliberately reversed here: the weekly window arrives as primary,
    the session window as secondary. Position must not decide the mapping.
    """
    usage = provider._usage_from_payload(
        _payload(
            {
                "allowed": True,
                "primary_window": {
                    "used_percent": 55,
                    "limit_window_seconds": 604800,
                    "reset_after_seconds": 86400,
                },
                "secondary_window": {
                    "used_percent": 12,
                    "limit_window_seconds": 18000,
                    "reset_after_seconds": 900,
                },
            }
        )
    )

    assert usage.session_pct == 12
    assert usage.session_window_s == 18000
    assert usage.weekly_pct == 55
    assert usage.weekly_window_s == 604800


def test_additional_rate_limits_are_scanned(provider):
    """Windows parked in additional_rate_limits still get classified."""
    usage = provider._usage_from_payload(
        _payload(
            {
                "allowed": True,
                "primary_window": {
                    "used_percent": 4,
                    "limit_window_seconds": 604800,
                    "reset_after_seconds": 500000,
                },
                "secondary_window": None,
            },
            additional_rate_limits=[
                {
                    "used_percent": 80,
                    "limit_window_seconds": 18000,
                    "reset_after_seconds": 1200,
                }
            ],
        )
    )

    assert usage.session_pct == 80
    assert usage.session_window_s == 18000
    assert usage.weekly_pct == 4


def test_missing_primary_window_is_an_error(provider):
    usage = provider._usage_from_payload(_payload({"allowed": True}))

    assert usage.ok is False
    assert usage.status == "codex_no_windows"


def test_unlimited_credits_render_short(provider):
    usage = provider._usage_from_payload(
        _payload(
            {
                "allowed": True,
                "primary_window": {
                    "used_percent": 1,
                    "limit_window_seconds": 604800,
                    "reset_after_seconds": 60,
                },
                "secondary_window": None,
            },
            credits={
                "has_credits": True,
                "unlimited": True,
                "balance": "0",
                "approx_local_messages": [120, 340],
            },
        )
    )

    # The firmware stores this in a 12-byte buffer.
    assert usage.credits_balance == "unlim."
    assert usage.credits_has is True
    assert usage.credits_msgs == 340


@pytest.mark.parametrize(
    "raw, expected",
    [
        ("407.4704980", "407.47"),
        ("12.345678", "12.35"),
        ("7", "7.00"),
        (3.14159, "3.14"),
        (1234.5, "1234.50"),
        ("n/a", "n/a"),
        (None, ""),
    ],
)
def test_credit_balance_rounded_to_two_decimals(raw, expected):
    assert CodexProvider._format_balance(raw) == expected
