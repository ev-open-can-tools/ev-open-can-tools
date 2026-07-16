import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class VSCodeIntellisenseTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.settings = json.loads(
            (ROOT / ".vscode" / "settings.json").read_text(encoding="utf-8")
        )
        cls.extensions = json.loads(
            (ROOT / ".vscode" / "extensions.json").read_text(encoding="utf-8")
        )
        cls.devcontainer = (
            ROOT / ".devcontainer" / "devcontainer.json"
        ).read_text(encoding="utf-8")
        cls.gitignore = (ROOT / ".gitignore").read_text(encoding="utf-8")
        cls.building = (ROOT / "docs" / "building.md").read_text(encoding="utf-8")

    def test_workspace_uses_platformio_configuration_provider(self) -> None:
        self.assertEqual(
            "platformio.platformio-ide",
            self.settings["C_Cpp.default.configurationProvider"],
        )
        self.assertEqual("cpp", self.settings["files.associations"]["*.h"])

    def test_recommended_extensions_cover_platformio_and_cpp(self) -> None:
        recommendations = self.extensions["recommendations"]
        self.assertIn("platformio.platformio-ide", recommendations)
        self.assertIn("ms-vscode.cpptools", recommendations)

    def test_devcontainer_uses_same_configuration_provider(self) -> None:
        provider = '"C_Cpp.default.configurationProvider": "platformio.platformio-ide"'
        self.assertIn(provider, self.devcontainer)

    def test_machine_specific_compilation_database_is_not_committed(self) -> None:
        self.assertFalse((ROOT / "compile_commands.json").exists())
        self.assertIn("compile_commands.json", self.gitignore.splitlines())

    def test_shared_vscode_files_are_not_hidden_by_gitignore(self) -> None:
        lines = self.gitignore.splitlines()
        self.assertIn(".vscode/*", lines)
        self.assertIn("!.vscode/settings.json", lines)
        self.assertIn("!.vscode/extensions.json", lines)
        self.assertNotIn(".vscode/", lines)
        self.assertNotIn("!.vscode/launch.json", lines)

    def test_documentation_explains_environment_switching(self) -> None:
        self.assertIn("## VS Code IntelliSense", self.building)
        self.assertIn("PlatformIO environment switcher", self.building)
        self.assertIn("machine-specific", self.building)


if __name__ == "__main__":
    unittest.main()
