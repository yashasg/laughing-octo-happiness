#pragma once

/// Acquire a GitHub auth token from `gh auth token` if no env var is set.
/// Sets COPILOT_GITHUB_TOKEN so downstream Copilot CLI invocations skip the
/// system keychain (avoids credential popup).
void ensure_github_token();
