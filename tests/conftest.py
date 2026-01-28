"""
Pytest configuration for VROOM E2E tests
"""

import pytest


def pytest_configure(config):
    """Configure pytest with custom markers"""
    config.addinivalue_line(
        "markers",
        "slow: marks tests as slow (run with pytest -m slow)"
    )
    config.addinivalue_line(
        "markers",
        "relations: marks tests for relation constraints"
    )
    config.addinivalue_line(
        "markers",
        "steps: marks tests for vehicle steps constraints"
    )


def pytest_addoption(parser):
    """Add custom command-line options"""
    parser.addoption(
        "--vroom-binary",
        action="store",
        default="../bin/vroom",
        help="Path to VROOM binary (default: ../bin/vroom)"
    )


@pytest.fixture(scope="session")
def vroom_binary_path(request):
    """Get VROOM binary path from command line option"""
    return request.config.getoption("--vroom-binary")


def pytest_collection_modifyitems(config, items):
    """Modify test collection to add markers automatically"""
    for item in items:
        # Add markers based on test file
        if "test_relations" in item.nodeid:
            item.add_marker(pytest.mark.relations)
        elif "test_vehicle_steps" in item.nodeid:
            item.add_marker(pytest.mark.steps)


def pytest_report_header(config):
    """Add custom header to pytest output"""
    return [
        "VROOM E2E Test Suite",
        f"VROOM Binary: {config.getoption('--vroom-binary')}",
    ]


@pytest.hookimpl(tryfirst=True, hookwrapper=True)
def pytest_runtest_makereport(item, call):
    """
    Make test results available to fixtures

    This allows us to access test results in fixtures for cleanup or logging.
    """
    outcome = yield
    rep = outcome.get_result()

    # Store test result on the item for later access
    setattr(item, f"rep_{rep.when}", rep)
