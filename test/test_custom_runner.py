import click
import re

from platformio.test.result import TestCase, TestStatus
from platformio.test.runners.base import TestRunnerBase


class CustomTestRunner(TestRunnerBase):
    def on_testing_line_output(self, line):
        line = line.strip()
        control = re.fullmatch(r"CONTROL: (\d+) PASS, (\d+) FAIL", line)
        if control:
            passed, failed = control.groups()
            line = f"{'FAIL' if int(failed) else 'PASS'} Regelung: {passed} Prüfungen, {failed} Fehler"
        if line.startswith(("PASS ", "FAIL ")):
            status = TestStatus.PASSED if line.startswith("PASS ") else TestStatus.FAILED
            case = TestCase(name=line[5:], status=status, stdout=line)
            self.test_suite.add_case(case)
            click.echo(case.humanize())
        elif line:
            click.echo(line)
