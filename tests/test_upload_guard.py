import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import MagicMock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("upload_guard", ROOT / "tools/upload_guard.py")
guard = importlib.util.module_from_spec(spec)
spec.loader.exec_module(guard)


class HostGuardTest(unittest.TestCase):
    def test_blocked_targets_alone_and_combined(self):
        for target in guard.BLOCKED_TARGETS:
            for targets in ([target], ["buildprog", target], ["clean", target]):
                with self.subTest(targets=targets):
                    with self.assertRaisesRegex(RuntimeError, "install-host"):
                        guard.check_targets(targets)

    def test_non_writing_targets(self):
        for targets in ([], ["buildprog"], ["clean"], ["monitor"], ["size"]):
            guard.check_targets(targets)

    def test_size_boundary(self):
        with tempfile.TemporaryDirectory() as directory:
            image = Path(directory) / "firmware.bin"
            for size in (guard.HOST_MAX_SIZE - 1, guard.HOST_MAX_SIZE,
                         guard.HOST_MAX_SIZE + 1):
                with image.open("wb") as stream:
                    stream.truncate(size)
                if size > guard.HOST_MAX_SIZE:
                    with self.assertRaisesRegex(RuntimeError, "4194304"):
                        guard.check_size(image)
                else:
                    guard.check_size(image)

    def test_missing_image_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(FileNotFoundError):
                guard.check_size(Path(directory) / "missing.bin")

    def test_guard_runs_before_build_hooks(self):
        env = MagicMock()
        with self.assertRaisesRegex(RuntimeError, "install-host"):
            guard.configure(env, ["upload"])
        self.assertEqual(env.mock_calls, [])

    def test_cached_image_has_always_run_size_check(self):
        with tempfile.TemporaryDirectory() as directory:
            image = Path(directory) / "firmware.bin"
            env = MagicMock()
            env.get.return_value = "program"
            env.subst.return_value = str(image)
            env.Alias.side_effect = lambda name, *args: name
            guard.configure(env, [])
            env.Replace.assert_called_once_with(PROGNAME="firmware")
            env.AlwaysBuild.assert_called_once_with("multifirm_check_size")
            env.Depends.assert_called_once_with("buildprog", "multifirm_check_size")
            callback = env.AddPostAction.call_args.args[1]
            self.assertIs(env.Alias.call_args_list[0].args[2], callback)
            with image.open("wb") as stream:
                stream.truncate(guard.HOST_MAX_SIZE + 1)
            with self.assertRaisesRegex(RuntimeError, "4194304"):
                callback([], [], env)


if __name__ == "__main__":
    unittest.main()
