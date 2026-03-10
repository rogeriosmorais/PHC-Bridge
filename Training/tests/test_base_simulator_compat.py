import types

from protomotions.simulator.base_simulator.compat import load_image_sequence_clip


def test_load_image_sequence_clip_prefers_moviepy_editor():
    imported = []
    clip_class = object()

    def fake_import_module(name):
        imported.append(name)
        if name == "moviepy.editor":
            return types.SimpleNamespace(ImageSequenceClip=clip_class)
        raise AssertionError(f"unexpected import: {name}")

    assert load_image_sequence_clip(import_module=fake_import_module) is clip_class
    assert imported == ["moviepy.editor"]


def test_load_image_sequence_clip_falls_back_to_moviepy_root():
    imported = []
    clip_class = object()

    def fake_import_module(name):
        imported.append(name)
        if name == "moviepy.editor":
            raise ModuleNotFoundError(name)
        if name == "moviepy":
            return types.SimpleNamespace(ImageSequenceClip=clip_class)
        raise AssertionError(f"unexpected import: {name}")

    assert load_image_sequence_clip(import_module=fake_import_module) is clip_class
    assert imported == ["moviepy.editor", "moviepy"]
