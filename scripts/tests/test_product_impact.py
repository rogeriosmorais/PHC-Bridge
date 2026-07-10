from __future__ import annotations

import unittest

from scripts.product_impact import ProductImpactConfigurationError, classify_product_impact


class ProductImpactTests(unittest.TestCase):
    def test_runtime_source_change_is_product_impacting(self) -> None:
        result = classify_product_impact(
            ["PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp"]
        )

        self.assertTrue(result.product_impacting)
        self.assertEqual(result.product_paths, result.changed_paths)
        self.assertFalse(result.may_claim_product_success_without_receipt)

    def test_documentation_only_change_may_complete_but_cannot_claim_product_success(self) -> None:
        result = classify_product_impact(["docs/standing-gate.md", "README.md"])

        self.assertFalse(result.product_impacting)
        self.assertTrue(result.implementation_completion_allowed)
        self.assertFalse(result.may_claim_product_success_without_receipt)

    def test_unknown_paths_fail_closed_as_product_impacting(self) -> None:
        result = classify_product_impact(["new-runtime-layout.bin"])

        self.assertTrue(result.product_impacting)
        self.assertEqual(result.unknown_paths, ("new-runtime-layout.bin",))

    def test_mixed_documentation_and_runtime_change_is_product_impacting(self) -> None:
        result = classify_product_impact(
            ["docs/notes.md", "scripts/product_gate.py"]
        )

        self.assertTrue(result.product_impacting)
        self.assertEqual(result.product_paths, ("scripts/product_gate.py",))

    def test_traversal_and_absolute_paths_are_rejected(self) -> None:
        for path in ("../outside.cpp", "F:/outside.cpp", "/outside.cpp"):
            with self.subTest(path=path):
                with self.assertRaises(ProductImpactConfigurationError):
                    classify_product_impact([path])


if __name__ == "__main__":
    unittest.main()
