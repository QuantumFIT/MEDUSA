"""Unit tests of .github/scripts/code_quality.py, the Code quality check.

The script decides whether a pull request fails, so its own logic is tested
here, without the tools it drives: `run` - the only place it starts a process
- is replaced by a fake that returns prepared tool output. Covered:

- the three parsers (compiler warnings, cppcheck, shellcheck);
- normalisation: findings do not depend on line numbers, nor on how a path
  is spelt, and duplicates count once;
- the comparison: new findings fail (exit 1, one annotation each), fixed and
  existing ones do not, and the job summary gets the table;
- the collectors refuse incomplete runs: a failed clean or build, `make -Bn`
  failing, cppcheck stopping, shellcheck unable to read a file;
- the base a change is compared against, for each event.

Run from the repository root: python3 -m unittest -v tests/test_code_quality.py
"""
import contextlib
import importlib.util
import io
import os
import subprocess
import tempfile
import unittest
from unittest import mock

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_spec = importlib.util.spec_from_file_location(
    "code_quality", os.path.join(ROOT, ".github", "scripts", "code_quality.py"))
cq = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(cq)


def done(returncode=0, stdout="", stderr=""):
    return subprocess.CompletedProcess([], returncode, stdout, stderr)


class FakeRun:
    """Stands in for code_quality.run: answers each command from a list of
    (prefix, CompletedProcess) pairs, the first matching prefix winning, and
    raises CalledProcessError for check=True on a non-zero exit, as
    subprocess.run does."""

    def __init__(self, answers):
        self.answers = answers
        self.calls = []

    def __call__(self, cmd, check=False, **kw):
        self.calls.append(list(cmd))
        for prefix, res in self.answers:
            if cmd[:len(prefix)] == prefix:
                if check and res.returncode != 0:
                    raise subprocess.CalledProcessError(res.returncode, cmd)
                return res
        raise AssertionError(f"unexpected command: {cmd}")


class InTempDir(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self._cwd = os.getcwd()
        os.chdir(self._tmp.name)
        self.addCleanup(self._tmp.cleanup)
        self.addCleanup(os.chdir, self._cwd)

    def quiet(self, fn, *args):
        """Calls fn, keeping the script's progress output out of the test log."""
        with contextlib.redirect_stdout(io.StringIO()):
            return fn(*args)


class Parsers(unittest.TestCase):
    def test_gcc_warning(self):
        m = cq.GCC_WARNING.match("src/sim.c:120:5: warning: unused variable 'x' [-Wunused-variable]")
        self.assertEqual((m["file"], m["msg"], m["flag"]), ("src/sim.c", "unused variable 'x'", "-Wunused-variable"))
        # without a flag, and not a note or an error
        self.assertIsNone(cq.GCC_WARNING.match("src/sim.c:1:1: warning: oops")["flag"])
        self.assertIsNone(cq.GCC_WARNING.match("src/sim.c:1:1: note: declared here"))
        self.assertIsNone(cq.GCC_WARNING.match("src/sim.c:1:1: error: expected ';'"))

    def test_cppcheck_line(self):
        m = cq.CPPCHECK_LINE.match("src/htab.c:42:warning:nullPointer:Possible null pointer dereference: t")
        self.assertEqual((m["file"], m["sev"], m["id"], m["msg"]),
                         ("src/htab.c", "warning", "nullPointer", "Possible null pointer dereference: t"))

    def test_shellcheck_line(self):
        m = cq.SHELLCHECK_LINE.match("tests/x.sh:7:3: warning: Quote this to prevent word splitting. [SC2046]")
        self.assertEqual((m["file"], m["sev"], m["id"]), ("tests/x.sh", "warning", "SC2046"))


class Normalisation(InTempDir):
    def test_message_line_references(self):
        self.assertEqual(cq.norm_msg(" shadows the one at line 347 "), "shadows the one at line N")
        self.assertEqual(cq.norm_msg("outline 3"), "outline 3")    # only the word "line"

    def test_paths(self):
        os.makedirs("src")
        self.assertEqual(cq.norm_path("src/../src/sim.c"), os.path.join("src", "sim.c"))
        self.assertEqual(cq.norm_path(os.path.abspath("src/sim.c")), os.path.join("src", "sim.c"))

    def test_moved_lines_are_the_same_finding(self):
        """A finding moved by an unrelated edit above it is not new."""
        before = cq.GCC_WARNING.match("src/a.c:10:1: warning: unused variable 'x' [-Wunused-variable]")
        after = cq.GCC_WARNING.match("src/a.c:57:9: warning: unused variable 'x' [-Wunused-variable]")
        key = lambda m: (cq.norm_path(m["file"]), m["flag"], cq.norm_msg(m["msg"]))
        self.assertEqual(key(before), key(after))

    def test_write_dedups_and_sorts(self):
        f = [("b.c", "-Wx", "m"), ("a.c", "-Wy", "n"), ("b.c", "-Wx", "m")]
        self.quiet(cq.write, f, "out.txt")
        with open("out.txt", encoding="utf-8") as fh:
            self.assertEqual(fh.read(), "a.c\t-Wy\tn\nb.c\t-Wx\tm\n")
        self.assertEqual(cq.read("out.txt"), {("a.c", "-Wy", "n"), ("b.c", "-Wx", "m")})


class Compare(InTempDir):
    def files(self, base, head):
        self.quiet(cq.write, base, "base.txt")
        self.quiet(cq.write, head, "head.txt")

    def run_compare(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out), mock.patch.dict(os.environ, {"GITHUB_STEP_SUMMARY": "summary.md"}):
            rc = cq.compare("gcc", "base.txt", "head.txt")
        return rc, out.getvalue()

    def test_new_fails_with_an_annotation_each(self):
        old, new1, new2 = ("a.c", "-Wx", "old"), ("b.c", "-Wy", "new one"), ("c.c", "-Wz", "new two")
        self.files([old], [old, new1, new2])
        rc, out = self.run_compare()
        self.assertEqual(rc, 1)
        self.assertEqual(out.count("::error "), 2)
        self.assertIn("::error file=b.c,title=gcc: new -Wy::new one", out)
        with open("summary.md", encoding="utf-8") as fh:
            summary = fh.read()
        self.assertIn("| **new** | **2** |", summary)
        self.assertIn("| fixed | 0 |", summary)

    def test_fixed_and_existing_pass(self):
        old, gone = ("a.c", "-Wx", "old"), ("b.c", "-Wy", "fixed")
        self.files([old, gone], [old])
        rc, out = self.run_compare()
        self.assertEqual(rc, 0)
        self.assertNotIn("::error", out)
        self.assertIn("| fixed | 1 |", out)
        self.assertIn("Fixed by this change", out)

    def test_nothing_on_either_side(self):
        self.files([], [])
        rc, out = self.run_compare()
        self.assertEqual(rc, 0)
        self.assertIn("| **new** | **0** |", out)


GOOD_BUILD = done(0, "src/sim.c:3:1: warning: unused variable 'x' [-Wunused-variable]\n"
                     "src/sim.c:9:1: warning: unused variable 'x' [-Wunused-variable]\n")


class Collectors(InTempDir):
    def collect(self, fn, fake, *args):
        with mock.patch.object(cq, "run", fake), contextlib.redirect_stdout(io.StringIO()):
            return fn(*args)

    def test_warnings_collected_once_per_finding(self):
        fake = FakeRun([(["make", "clean-artifacts"], done()), (["make"], GOOD_BUILD)])
        self.collect(cq.collect_warnings, fake, "clang", "w.txt")
        self.assertEqual(cq.read("w.txt"), {("src/sim.c", "-Wunused-variable", "unused variable 'x'")})
        self.assertEqual(fake.calls[0], ["make", "clean-artifacts"])

    def test_warnings_failed_clean(self):
        fake = FakeRun([(["make", "clean-artifacts"], done(2)), (["make"], GOOD_BUILD)])
        with self.assertRaises(subprocess.CalledProcessError):
            self.collect(cq.collect_warnings, fake, "gcc", "w.txt")
        self.assertFalse(os.path.exists("w.txt"))

    def test_warnings_failed_build(self):
        fake = FakeRun([(["make", "clean-artifacts"], done()),
                        (["make"], done(2, "src/x.c:1:1: error: expected ';'\nmake: *** [x.o] Error 1\n"))])
        with self.assertRaises(SystemExit) as e:
            self.collect(cq.collect_warnings, fake, "gcc", "w.txt")
        self.assertIn("build failed", str(e.exception.code))
        self.assertFalse(os.path.exists("w.txt"))

    def test_compile_commands_parsed(self):
        out = ("gcc -I src -Isrc/inc -D LEAF=3 -DX -include src/h.h -c src/sim.c -o obj/sim.o\n"
               "g++ -c src/gates.cpp -o obj/gates.o\n"
               "gcc -c lib/a.s -o a.o\n"
               "echo done\n")
        fake = FakeRun([(["make", "-Bn"], done(0, out))])
        units = self.collect(cq.compile_commands, fake, "buddy_doubles_f128")
        self.assertEqual(units, [("src/sim.c", ["-Isrc", "-Isrc/inc", "-DLEAF=3", "-DX"], ["src/h.h"])])

    def test_compile_commands_make_fails(self):
        fake = FakeRun([(["make", "-Bn"], done(2, "gcc -c src/a.c\n", "No rule to make target"))])
        with self.assertRaises(SystemExit) as e:
            self.collect(cq.compile_commands, fake, "buddy_gmp")
        self.assertIn("make -Bn buddy_gmp failed", str(e.exception.code))

    def test_cppcheck_stopped(self):
        fake = FakeRun([(["make", "-Bn"], done(0, "gcc -c src/a.c -o a.o\n")),
                        (["cppcheck"], done(1, "", "src/a.c:1:error:syntaxError:..."))])
        with self.assertRaises(SystemExit) as e:
            self.collect(cq.collect_cppcheck, fake, "c.txt")
        self.assertIn("cppcheck failed on src/a.c", str(e.exception.code))
        self.assertFalse(os.path.exists("c.txt"))

    def test_cppcheck_no_compile_commands(self):
        fake = FakeRun([(["make", "-Bn"], done(0, "echo nothing\n"))])
        with self.assertRaises(SystemExit) as e:
            self.collect(cq.collect_cppcheck, fake, "c.txt")
        self.assertIn("no compile commands", str(e.exception.code))

    def test_cppcheck_findings(self):
        fake = FakeRun([(["make", "-Bn"], done(0, "gcc -c src/a.c -o a.o\n")),
                        (["cppcheck"], done(0, "", "src/a.c:5:warning:nullPointer:Possible null\n"))])
        self.collect(cq.collect_cppcheck, fake, "c.txt")
        # one per variant, the same finding: counted once
        self.assertEqual(cq.read("c.txt"), {("src/a.c", "warning:nullPointer", "Possible null")})

    def test_shellcheck_unreadable_file(self):
        fake = FakeRun([(["git"], done(0, "a.sh\0b.sh\0")),
                        (["shellcheck"], done(2, "b.sh: openBinaryFile: does not exist"))])
        with self.assertRaises(SystemExit) as e:
            self.collect(cq.collect_shellcheck, fake, "s.txt")
        self.assertIn("shellcheck failed (2)", str(e.exception.code))

    def test_shellcheck_findings_and_paths_with_spaces(self):
        fake = FakeRun([(["git"], done(0, "a.sh\0dir/with space.sh\0")),
                        (["shellcheck"], done(1, "a.sh:3:1: warning: Quote this. [SC2086]\n"))])
        self.collect(cq.collect_shellcheck, fake, "s.txt")
        self.assertEqual(fake.calls[1][-2:], ["a.sh", "dir/with space.sh"])
        self.assertEqual(cq.read("s.txt"), {("a.sh", "SC2086", "Quote this.")})


class BaseRevision(unittest.TestCase):
    def test_events(self):
        sha, before = "a" * 40, "b" * 40
        self.assertEqual(cq.base_revision("pull_request", sha, before), sha)
        self.assertEqual(cq.base_revision("push", sha, before), before)
        # a push that created the branch: GitHub reports an all-zero before
        self.assertEqual(cq.base_revision("push", "", cq.NULL_SHA), "devel")
        self.assertEqual(cq.base_revision("push", "", ""), "devel")
        self.assertEqual(cq.base_revision("workflow_dispatch", "", ""), "devel")
        # a pull_request event without its base is not taken at face value
        self.assertEqual(cq.base_revision("pull_request", "", before), "devel")


if __name__ == "__main__":
    unittest.main()
