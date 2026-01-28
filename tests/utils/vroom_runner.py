"""VROOM binary execution wrapper"""

import json
import subprocess
import tempfile
from pathlib import Path
from typing import Dict, Optional


class VroomRunnerError(Exception):
    """Raised when VROOM execution fails"""
    pass


class VroomRunner:
    """Execute VROOM binary and parse results"""

    def __init__(self, binary_path: str = "bin/vroom"):
        """
        Initialize VROOM runner

        Args:
            binary_path: Path to VROOM binary (relative to project root)
        """
        # Get project root (parent of tests directory)
        tests_dir = Path(__file__).parent.parent
        project_root = tests_dir.parent

        # Handle both absolute and relative paths
        if Path(binary_path).is_absolute():
            self.binary_path = Path(binary_path)
        else:
            self.binary_path = project_root / binary_path

        self._validate_binary()

    def _validate_binary(self) -> None:
        """Validate that VROOM binary exists and is executable"""
        if not self.binary_path.exists():
            raise VroomRunnerError(
                f"VROOM binary not found at {self.binary_path}. "
                f"Build it with: cd src && make clean && make -j4 USE_ROUTING=false"
            )

        if not self.binary_path.is_file():
            raise VroomRunnerError(f"{self.binary_path} is not a file")

    def run(self, input_file: str) -> Dict:
        """
        Execute VROOM with JSON input file

        Args:
            input_file: Path to input JSON file (absolute or relative to tests/)

        Returns:
            Parsed output JSON as dictionary

        Raises:
            VroomRunnerError: If execution fails or output is invalid
        """
        input_path = Path(input_file)
        if not input_path.is_absolute():
            input_path = Path(__file__).parent.parent / input_path

        if not input_path.exists():
            raise VroomRunnerError(f"Input file not found: {input_path}")

        try:
            result = subprocess.run(
                [str(self.binary_path), "-i", str(input_path)],
                capture_output=True,
                text=True,
                timeout=30,
                check=False,
            )

            # VROOM may write to stderr for warnings, but that's ok
            # as long as stdout has valid JSON
            if not result.stdout.strip():
                raise VroomRunnerError(
                    f"VROOM produced no output. Stderr: {result.stderr}"
                )

            return self._parse_output(result.stdout)

        except subprocess.TimeoutExpired:
            raise VroomRunnerError(
                f"VROOM execution timed out after 30s for {input_file}"
            )
        except subprocess.SubprocessError as e:
            raise VroomRunnerError(f"Failed to execute VROOM: {e}")

    def run_with_json(self, input_data: Dict) -> Dict:
        """
        Execute VROOM with in-memory JSON data

        Args:
            input_data: Input JSON as dictionary

        Returns:
            Parsed output JSON as dictionary

        Raises:
            VroomRunnerError: If execution fails or output is invalid
        """
        # Write to temporary file
        with tempfile.NamedTemporaryFile(
            mode="w", suffix=".json", delete=False
        ) as tmp_file:
            json.dump(input_data, tmp_file, indent=2)
            tmp_path = tmp_file.name

        try:
            return self.run(tmp_path)
        finally:
            # Clean up temporary file
            Path(tmp_path).unlink(missing_ok=True)

    def _parse_output(self, output: str) -> Dict:
        """
        Parse VROOM JSON output

        Args:
            output: Raw JSON output from VROOM

        Returns:
            Parsed JSON as dictionary

        Raises:
            VroomRunnerError: If output is not valid JSON
        """
        try:
            data = json.loads(output)
            self._validate_output_structure(data)
            return data
        except json.JSONDecodeError as e:
            raise VroomRunnerError(f"Invalid JSON output from VROOM: {e}")

    def _validate_output_structure(self, data: Dict) -> None:
        """
        Validate that output has expected structure

        Args:
            data: Parsed JSON output

        Raises:
            VroomRunnerError: If structure is invalid
        """
        if not isinstance(data, dict):
            raise VroomRunnerError("Output is not a JSON object")

        required_fields = ["code", "summary"]
        missing = [f for f in required_fields if f not in data]
        if missing:
            raise VroomRunnerError(
                f"Output missing required fields: {', '.join(missing)}"
            )

        # code 0 = success, non-zero = error
        if data["code"] != 0:
            error_msg = data.get("error", "Unknown error")
            raise VroomRunnerError(f"VROOM returned error code {data['code']}: {error_msg}")

        # Check for routes or unassigned (at least one should exist)
        if "routes" not in data and "unassigned" not in data:
            raise VroomRunnerError("Output has no 'routes' or 'unassigned' field")
