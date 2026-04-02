"""Tests for status_monitor module — event parsing logic."""

import json
import tempfile
from pathlib import Path
from unittest.mock import patch, MagicMock

from config import CopilotStatus
from status_monitor import StatusMonitor, _STATUS_MAP


class TestStatusMap:
    """Verify the event-type → status mapping is correct."""

    def test_busy_events(self):
        busy_events = [
            "assistant.turn_start",
            "tool.execution_start",
            "assistant.message",
            "hook.start",
        ]
        for event_type in busy_events:
            assert _STATUS_MAP[event_type] == CopilotStatus.BUSY, (
                f"{event_type} should map to BUSY"
            )

    def test_waiting_events(self):
        assert _STATUS_MAP["assistant.turn_end"] == CopilotStatus.WAITING

    def test_idle_events(self):
        assert _STATUS_MAP["session.task_complete"] == CopilotStatus.IDLE

    def test_all_mapped_statuses_are_valid(self):
        for event_type, status in _STATUS_MAP.items():
            assert isinstance(status, CopilotStatus)


class TestParseStatus:
    """Test _parse_status with real temp files."""

    def _make_monitor(self):
        callback = MagicMock()
        monitor = StatusMonitor(on_status_change=callback)
        return monitor

    def _write_events(self, tmpdir: Path, events: list[dict]) -> Path:
        session_dir = tmpdir / "test-session"
        session_dir.mkdir(parents=True, exist_ok=True)
        events_file = session_dir / "events.jsonl"
        with open(events_file, "w") as f:
            for event in events:
                f.write(json.dumps(event) + "\n")
        return session_dir

    def test_empty_events_file(self, tmp_path):
        monitor = self._make_monitor()
        session_dir = tmp_path / "test-session"
        session_dir.mkdir()
        (session_dir / "events.jsonl").write_text("")
        status, text = monitor._parse_status(session_dir)
        assert status == CopilotStatus.IDLE

    def test_busy_status_from_turn_start(self, tmp_path):
        monitor = self._make_monitor()
        events = [
            {"type": "session.start", "data": {"selectedModel": "claude-opus-4.6"}},
            {"type": "assistant.turn_start", "data": {}},
        ]
        session_dir = self._write_events(tmp_path, events)
        status, text = monitor._parse_status(session_dir)
        assert status == CopilotStatus.BUSY

    def test_waiting_status_from_turn_end(self, tmp_path):
        monitor = self._make_monitor()
        events = [
            {"type": "assistant.turn_start", "data": {}},
            {"type": "assistant.turn_end", "data": {}},
        ]
        session_dir = self._write_events(tmp_path, events)
        status, text = monitor._parse_status(session_dir)
        assert status == CopilotStatus.WAITING

    def test_idle_status_from_task_complete(self, tmp_path):
        monitor = self._make_monitor()
        events = [
            {"type": "assistant.turn_start", "data": {}},
            {"type": "assistant.turn_end", "data": {}},
            {"type": "session.task_complete", "data": {}},
        ]
        session_dir = self._write_events(tmp_path, events)
        status, text = monitor._parse_status(session_dir)
        assert status == CopilotStatus.IDLE

    def test_intent_text_extraction(self, tmp_path):
        monitor = self._make_monitor()
        events = [
            {"type": "assistant.turn_start", "data": {}},
            {
                "type": "tool.execution_start",
                "data": {
                    "toolName": "report_intent",
                    "arguments": {"intent": "Fixing bug"},
                },
            },
        ]
        session_dir = self._write_events(tmp_path, events)
        status, text = monitor._parse_status(session_dir)
        assert status == CopilotStatus.BUSY
        assert text == "Fixing bug"

    def test_model_name_extraction(self, tmp_path):
        monitor = self._make_monitor()
        events = [
            {"type": "session.start", "data": {"selectedModel": "claude-opus-4.6"}},
            {"type": "assistant.turn_start", "data": {}},
        ]
        session_dir = self._write_events(tmp_path, events)
        monitor._parse_status(session_dir)
        assert monitor.model_name == "claude-opus-4.6"

    def test_file_size_tracking(self, tmp_path):
        monitor = self._make_monitor()
        events = [
            {"type": "assistant.turn_start", "data": {}},
        ]
        session_dir = self._write_events(tmp_path, events)
        monitor._parse_status(session_dir)
        assert monitor.context_bytes > 0


class TestFindActiveSession:
    """Test _find_active_session with mock directories."""

    def test_no_state_dir(self, tmp_path):
        monitor = StatusMonitor(on_status_change=MagicMock())
        nonexistent = tmp_path / "nonexistent"
        with patch("status_monitor.COPILOT_STATE_DIR", nonexistent):
            result = monitor._find_active_session()
        assert result is None

    def test_empty_state_dir(self, tmp_path):
        monitor = StatusMonitor(on_status_change=MagicMock())
        with patch("status_monitor.COPILOT_STATE_DIR", tmp_path):
            result = monitor._find_active_session()
        assert result is None

    def test_session_with_lock_and_events(self, tmp_path):
        monitor = StatusMonitor(on_status_change=MagicMock())
        session_dir = tmp_path / "abc-123"
        session_dir.mkdir()
        (session_dir / "inuse.12345.lock").write_text("")
        (session_dir / "events.jsonl").write_text('{"type":"assistant.turn_start","data":{}}\n')

        with patch("status_monitor.COPILOT_STATE_DIR", tmp_path):
            result = monitor._find_active_session()
        assert result == session_dir

    def test_session_without_lock_is_ignored(self, tmp_path):
        monitor = StatusMonitor(on_status_change=MagicMock())
        session_dir = tmp_path / "abc-123"
        session_dir.mkdir()
        (session_dir / "events.jsonl").write_text('{"type":"assistant.turn_start","data":{}}\n')

        with patch("status_monitor.COPILOT_STATE_DIR", tmp_path):
            result = monitor._find_active_session()
        assert result is None
