"""Usage provider interfaces for the Clawdmeter host daemon."""

from dataclasses import dataclass
from typing import Protocol


@dataclass(frozen=True)
class Credentials:
    access_token: str
    refresh_token: str | None = None
    account_id: str | None = None
    storage: str | None = None


@dataclass(frozen=True)
class Usage:
    session_pct: int
    session_reset_min: int
    weekly_pct: int
    weekly_reset_min: int
    status: str
    ok: bool
    # Length of each rate-limit window in seconds; 0 means the provider does not
    # expose that window at all. Distinguishes "window absent" from "0% used" —
    # ChatGPT dropped its 5h window in July 2026 and now reports weekly only.
    session_window_s: int = 0
    weekly_window_s: int = 0
    # Prepaid credit balance. Only ChatGPT reports this; credits_valid stays
    # False elsewhere so consumers can skip the field entirely.
    credits_valid: bool = False
    credits_has: bool = False
    credits_balance: str = ""
    credits_msgs: int = 0


class Provider(Protocol):
    name: str

    def read_credentials(self) -> Credentials | None:
        ...

    async def fetch_usage(self) -> Usage | None:
        ...
