#pragma once

#include <string>
#include <unordered_map>

/// Parse a JSON response body from the GitHub Copilot /models API endpoint and
/// return a map of model_id → context_window_tokens.
/// Handles both the OpenAI "data" array format and a "models" array format.
/// Returns an empty map on parse failure or when no usable data is present.
/// Exposed as a free function so it can be unit-tested without a network call.
std::unordered_map<std::string, size_t> parse_copilot_models_response(const std::string& json_body);

/// Fetch model context window sizes from the GitHub Copilot models API.
/// Makes a GET request to https://api.githubcopilot.com/models using the first
/// available GitHub token from the environment (COPILOT_GITHUB_TOKEN, GH_TOKEN,
/// GITHUB_TOKEN — in that priority order).
/// Returns a map of model_id → context_window_tokens on success, empty map on
/// failure (no token, network error, curl unavailable, unexpected response).
std::unordered_map<std::string, size_t> fetch_copilot_model_limits();
