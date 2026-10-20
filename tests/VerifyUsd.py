"""Validate the C++ placement output with actual OpenUSD parsing and composition."""
from pathlib import Path
import sys
import unittest

from pxr import Gf, Sdf, Usd, UsdGeom

ROOT = Path(sys.argv[1]).resolve()


class PlacementTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assets = ROOT / "assets"
        assets.mkdir(exist_ok=True)
        model_stage = Usd.Stage.CreateNew(str(assets / "model@v1.usda"))
        model = UsdGeom.Xform.Define(model_stage, "/Model")
        UsdGeom.XformCommonAPI(model).SetTranslate(Gf.Vec3d(0.5, 0, 0))
        UsdGeom.Cube.Define(model_stage, "/Model/Cube").GetSizeAttr().Set(2.0)
        model_stage.SetDefaultPrim(model.GetPrim())
        UsdGeom.SetStageUpAxis(model_stage, UsdGeom.Tokens.z)
        UsdGeom.SetStageMetersPerUnit(model_stage, 1.0)
        model_stage.GetRootLayer().Save()
        shape_stage = Usd.Stage.CreateNew(str(assets / "shape.usda"))
        shape = UsdGeom.Sphere.Define(shape_stage, "/Shape")
        shape.GetRadiusAttr().Set(0.5)
        shape.AddTranslateOp().Set(Gf.Vec3d(0, 1, 0))
        UsdGeom.SetStageUpAxis(shape_stage, UsdGeom.Tokens.z)
        UsdGeom.SetStageMetersPerUnit(shape_stage, 1.0)
        shape_stage.GetRootLayer().Save()
        cls.stage = Usd.Stage.Open(str(ROOT / "fixture.usda"))
        if not cls.stage:
            raise AssertionError("OpenUSD could not open the C++ placement output")

    def assertVector(self, actual, expected):
        for a, b in zip(actual, expected):
            self.assertAlmostEqual(a, b, delta=1e-9)

    def test_stage_and_metadata(self):
        stage = self.stage
        self.assertEqual(str(stage.GetDefaultPrim().GetPath()), "/World")
        self.assertEqual(UsdGeom.GetStageUpAxis(stage), UsdGeom.Tokens.z)
        self.assertEqual(UsdGeom.GetStageMetersPerUnit(stage), 1.0)
        player = stage.GetPrimAtPath("/World/Player")
        original_name = (ROOT / "player-name.txt").read_text(encoding="utf-8")
        self.assertEqual(player.GetMetadata("displayName"), original_name)
        self.assertEqual(player.GetAttribute("iiActor:name").Get(), original_name)
        self.assertGreater(player.GetAttribute("iiActor:id").Get(), 0)
        self.assertEqual(player.GetAttribute("iiActor:state").Get(), "playing")
        self.assertFalse(player.GetAttribute("iiActor:tickEnabled").Get())
        self.assertFalse(player.GetAttribute("iiActor:componentActive").Get())
        self.assertEqual(list(player.GetAttribute("iiActor:tags").Get()), [
            "Interactive", '" ) { def Xform "Injected" {} } //\n', "control:\x01"])
        self.assertFalse(any(p.GetName() == "Injected" for p in stage.Traverse()))
        behavior = stage.GetPrimAtPath("/World/Player/Component_4")
        self.assertEqual(behavior.GetTypeName(), "Scope")
        self.assertEqual(behavior.GetAttribute("iiActor:componentName").Get(), "Behavior\nline")
        self.assertEqual(behavior.GetAttribute("iiActor:componentType").Get(), "behavior")
        self.assertFalse(behavior.GetAttribute("iiActor:componentActive").Get())
        self.assertFalse(behavior.GetAttribute("iiActor:componentTickEnabled").Get())

    def test_matrices_hierarchy_and_detachment(self):
        cache = UsdGeom.XformCache(Usd.TimeCode.Default())
        rows = (ROOT / "points.tsv").read_text(encoding="utf-8").splitlines()
        self.assertEqual(len(rows), 15)  # Five spatial prims, three independent points each.
        for row in rows:
            path, *values = row.split("\t")
            values = list(map(float, values))
            prim = self.stage.GetPrimAtPath(path)
            self.assertTrue(prim, path)
            transformed = cache.GetLocalToWorldTransform(prim).Transform(Gf.Vec3d(*values[:3]))
            self.assertVector(transformed, values[3:])
        child = self.stage.GetPrimAtPath("/World/Player/Component_3/Component_2")
        self.assertEqual(str(child.GetParent().GetPath()), "/World/Player/Component_3")
        detached = self.stage.GetPrimAtPath("/World/Player/Component_1")
        self.assertTrue(UsdGeom.Xformable(detached).GetResetXformStack())
        self.assertVector(cache.GetLocalToWorldTransform(detached).ExtractTranslation(), (100, 200, 300))

    def test_reference_schemas_instancing_and_transforms(self):
        cache = UsdGeom.XformCache()
        model = self.stage.GetPrimAtPath("/World/Player/Asset_0")
        self.assertTrue(model.IsInstance())
        cube = self.stage.GetPrimAtPath("/World/Player/Asset_0/Cube")
        self.assertTrue(cube.IsA(UsdGeom.Cube))
        self.assertVector(cache.GetLocalToWorldTransform(cube).ExtractTranslation(), (10, 21, 30))
        sphere = self.stage.GetPrimAtPath("/World/Player/Component_3/Asset_1")
        self.assertTrue(sphere.IsA(UsdGeom.Sphere))
        self.assertEqual(sphere.GetAttribute("radius").Get(), 0.5)
        parent = sphere.GetParent()
        expected = cache.GetLocalToWorldTransform(parent).Transform(Gf.Vec3d(0, 1, 0))
        self.assertVector(cache.GetLocalToWorldTransform(sphere).ExtractTranslation(), expected)
        detached_cube = self.stage.GetPrimAtPath("/World/Player/Component_1/Asset_2/Cube")
        self.assertTrue(detached_cube.IsA(UsdGeom.Cube))
        self.assertVector(cache.GetLocalToWorldTransform(detached_cube).ExtractTranslation(), (100.5, 200, 300))
        bounds = UsdGeom.BBoxCache(Usd.TimeCode.Default(), [UsdGeom.Tokens.default_])
        self.assertFalse(bounds.ComputeWorldBound(self.stage.GetDefaultPrim()).GetRange().IsEmpty())

    def test_placement_layer_is_referenceable(self):
        shot = Usd.Stage.CreateInMemory()
        placed = UsdGeom.Xform.Define(shot, "/Shot/Placed")
        self.assertTrue(placed.GetPrim().GetReferences().AddReference(str(ROOT / "fixture.usda")))
        placed.AddTranslateOp().Set(Gf.Vec3d(1000, 0, 0))
        player = shot.GetPrimAtPath("/Shot/Placed/Player")
        self.assertTrue(player.IsA(UsdGeom.Xform))
        self.assertVector(UsdGeom.XformCache().GetLocalToWorldTransform(player).ExtractTranslation(), (1010, 20, 30))
        self.assertTrue(shot.GetPrimAtPath("/Shot/Placed/Player/Asset_0/Cube"))

    def test_alternate_axis_units_and_empty_layer(self):
        stage = Usd.Stage.Open(str(ROOT / "fixture-y.usda"))
        self.assertEqual(str(stage.GetDefaultPrim().GetPath()), "/Scene")
        self.assertEqual(UsdGeom.GetStageUpAxis(stage), UsdGeom.Tokens.y)
        self.assertEqual(UsdGeom.GetStageMetersPerUnit(stage), 0.01)
        self.assertVector(UsdGeom.XformCache().GetLocalToWorldTransform(
            stage.GetPrimAtPath("/Scene/Player")).ExtractTranslation(), (10, 20, 30))
        empty = Usd.Stage.Open(str(ROOT / "fixture-empty.usda"))
        self.assertEqual(str(empty.GetDefaultPrim().GetPath()), "/World")
        self.assertEqual(list(empty.GetDefaultPrim().GetChildren()), [])

    def test_usdc_roundtrip(self):
        binary = ROOT / "fixture.usdc"
        self.assertTrue(self.stage.GetRootLayer().Export(str(binary)))
        reopened = Usd.Stage.Open(str(binary))
        self.assertTrue(reopened.GetPrimAtPath("/World/Player/Asset_0/Cube"))
        player = reopened.GetPrimAtPath("/World/Player")
        self.assertEqual(player.GetAttribute("iiActor:name").Get(), (ROOT / "player-name.txt").read_text(encoding="utf-8"))
        self.assertVector(UsdGeom.XformCache().GetLocalToWorldTransform(player).ExtractTranslation(), (10, 20, 30))


if __name__ == "__main__":
    print("OpenUSD version:", Usd.GetVersion(), flush=True)
    unittest.main(argv=[sys.argv[0]], verbosity=2)
