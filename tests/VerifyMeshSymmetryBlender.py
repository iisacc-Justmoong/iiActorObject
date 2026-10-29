"""Verify mirrored shapes in actual Blender, independently of SDK import metadata."""
from pathlib import Path
import json
import sys
import bpy
from mathutils import Vector

directory = Path(sys.argv[sys.argv.index('--') + 1])
convert = lambda p: Vector((p[0], -p[2], p[1]))
correspondence = json.loads((directory / 'SymmetryCorrespondence.json').read_text())
results = []
for label in ('adult', 'child'):
    reference = json.loads((directory / label / 'SymmetryHumanReference.json').read_text())
    for extension, key in (('glb', 'renderPoints'), ('fbx', 'sourcePoints')):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        filename = str(directory / label / ('SymmetryHuman.' + extension))
        result = bpy.ops.import_scene.gltf(filepath=filename) if extension == 'glb' else bpy.ops.import_scene.fbx(filepath=filename)
        assert result == {'FINISHED'}
        meshes = [obj for obj in bpy.context.scene.objects if obj.type == 'MESH' and any(mod.type == 'ARMATURE' for mod in obj.modifiers)]
        rigs = [obj for obj in bpy.context.scene.objects if obj.type == 'ARMATURE']
        assert len(meshes) == 4 and len(rigs) == 1 and len(rigs[0].data.bones) == 163
        counts = {}
        for expected in reference['meshes']:
            obj = next(obj for obj in meshes if obj.name == expected['name'])
            assert obj.data.shape_keys
            assert set(obj.data.shape_keys.key_blocks.keys()) == {'Basis', *expected['morphs']}, (label, extension, obj.name)
            counts[obj.name] = len(expected['morphs'])
            for block in obj.data.shape_keys.key_blocks:
                block.value = 0
        def positions(obj):
            evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
            mesh = evaluated.to_mesh()
            values = [evaluated.matrix_world @ vertex.co for vertex in mesh.vertices]
            evaluated.to_mesh_clear()
            return values
        def check(expected_meshes):
            errors = {}
            for expected in expected_meshes:
                obj = next(obj for obj in meshes if obj.name == expected['name'])
                actual = positions(obj)
                assert len(actual) == len(expected[key]), (label, extension, obj.name, len(actual), len(expected[key]))
                error = max((a-convert(b)).length for a, b in zip(actual, expected[key]))
                assert error < 2e-6, (label, extension, obj.name, error)
                errors[obj.name] = error
            return errors
        neutral = check(reference['meshes'])
        errors = {}
        for unit in reference['units']:
            for obj in meshes:
                block = obj.data.shape_keys.key_blocks.get(unit['name'])
                if block:
                    block.value = 1
            bpy.context.view_layer.update()
            errors[unit['name']] = check(unit['meshes'])
            for obj in meshes:
                block = obj.data.shape_keys.key_blocks.get(unit['name'])
                if block:
                    block.value = 0
            bpy.context.view_layer.update()
        def activate(name):
            for obj in meshes:
                for block in obj.data.shape_keys.key_blocks:
                    block.value = 1 if block.name == name else 0
            bpy.context.view_layer.update()
        activate(None)
        baseline = {obj.name: positions(obj) for obj in meshes}
        all_errors, point_comparisons, skipped = {}, 0, 0
        for pair in correspondence['pairs']:
            activate(pair['source'])
            source_values = {obj.name: positions(obj) for obj in meshes}
            activate(pair['target'])
            target_values = {obj.name: positions(obj) for obj in meshes}
            maximum = 0.
            for mapping in correspondence['meshes']:
                name = mapping['name']
                neutral_values = baseline[name]
                source_points = source_values[name]
                target_points = target_values[name]
                point_ids = list(range(len(mapping['mirrors']))) if extension == 'fbx' else mapping['renderToPoint']
                first_render = {}
                for index, point in enumerate(point_ids):
                    first_render.setdefault(point, index)
                assert len(point_ids) == len(target_points)
                normal = convert(mapping['normal']).normalized()
                for index, point in enumerate(point_ids):
                    mirrored = mapping['mirrors'][point]
                    source_index = first_render.get(mirrored)
                    if source_index is None:
                        skipped += 1
                        continue
                    delta = source_points[source_index] - neutral_values[source_index]
                    expected = delta - 2 * delta.dot(normal) * normal
                    actual = target_points[index] - neutral_values[index]
                    error = (actual - expected).length
                    assert error < 4e-6, (label, extension, name, pair, index, error)
                    maximum = max(maximum, error)
                    point_comparisons += 1
            all_errors[pair['target']] = maximum
        activate(None)
        bpy.ops.wm.save_as_mainfile(filepath=str(directory / label / ('SymmetryHuman-' + extension + '.blend')))
        results.append({'shape': label, 'format': extension, 'meshes': 4, 'bones': 163, 'morphCounts': counts,
                        'neutralPositionErrorsMetres': neutral, 'mirroredPositionErrorsMetres': errors,
                        'all59MirrorVectorErrorsMetres': all_errors, 'mirrorVectorPointComparisons': point_comparisons,
                        'unrenderedCounterparts': skipped})
report = {'blender': bpy.app.version_string, 'results': results}
(directory / 'BlenderSymmetryVerification.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({'blender': report['blender'], 'cases': len(results), 'pairChecks': sum(len(r['all59MirrorVectorErrorsMetres']) for r in results), 'pointComparisons': sum(r['mirrorVectorPointComparisons'] for r in results), 'maximumMirrorVectorErrorMetres': max(v for r in results for v in r['all59MirrorVectorErrorsMetres'].values())}))
