import types

from protomotions.simulator.isaaclab.compat import (
    ISAACLAB_DISABLED_EXTENSIONS,
    build_isaaclab_kit_args,
    create_se2_keyboard,
    load_isaaclab_app_launcher,
)


def test_load_isaaclab_app_launcher_imports_h5py_first():
    imported = []

    def fake_import_module(name):
        imported.append(name)
        if name == "isaaclab.app":
            return types.SimpleNamespace(AppLauncher="launcher")
        return types.SimpleNamespace()

    launcher = load_isaaclab_app_launcher(import_module=fake_import_module)

    assert launcher == "launcher"
    assert imported == ["h5py", "isaaclab.app"]


def test_load_isaaclab_app_launcher_falls_back_to_source_module():
    imported = []

    def fake_import_module(name):
        imported.append(name)
        if name == "isaaclab.app":
            return types.SimpleNamespace()
        if name == "isaaclab.source.isaaclab.isaaclab.app.app_launcher":
            return types.SimpleNamespace(AppLauncher="launcher")
        return types.SimpleNamespace()

    launcher = load_isaaclab_app_launcher(import_module=fake_import_module)

    assert launcher == "launcher"
    assert imported == [
        "h5py",
        "isaaclab.app",
        "isaaclab.source.isaaclab.isaaclab.app.app_launcher",
    ]


def test_build_isaaclab_kit_args_excludes_broken_sensor_extensions():
    kit_args = build_isaaclab_kit_args("--/foo=bar")

    for index, extension_name in enumerate(ISAACLAB_DISABLED_EXTENSIONS):
        assert f"--/app/extensions/excluded/{index}={extension_name}" in kit_args

    assert kit_args.endswith("--/foo=bar")


def test_create_se2_keyboard_passes_cfg_to_keyboard():
    calls = {}

    class FakeCfg:
        def __init__(self, sim_device):
            calls["sim_device"] = sim_device

    class FakeKeyboard:
        def __init__(self, cfg):
            calls["cfg"] = cfg

    keyboard = create_se2_keyboard("cuda:0", keyboard_cls=FakeKeyboard, cfg_cls=FakeCfg)

    assert calls["sim_device"] == "cuda:0"
    assert isinstance(calls["cfg"], FakeCfg)
    assert isinstance(keyboard, FakeKeyboard)
