"""Check actual imported garment triangles, skin and neutral references in Blender."""
from collections import defaultdict
from itertools import product
from pathlib import Path
import hashlib
import json
import math
import sys

import bpy
import numpy as np
from mathutils import Quaternion, Vector, geometry
from mathutils.bvhtree import BVHTree


directory = Path(sys.argv[sys.argv.index('--') + 1])
convert = lambda p: Vector((p[0], -p[2], p[1]))


def evaluated_geometry(obj):
    evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
    mesh = evaluated.to_mesh()
    mesh.calc_loop_triangles()
    points = [evaluated.matrix_world @ v.co for v in mesh.vertices]
    triangles = [tuple(face.vertices) for face in mesh.loop_triangles]
    evaluated.to_mesh_clear()
    return points, triangles


def segment_distance_squared(p, q, a, b):
    # Independent clamped segment-segment calculation, including endpoint cases.
    u, v, w = q-p, b-a, p-a
    aa, bb, cc, dd, ee = u.dot(u), u.dot(v), v.dot(v), u.dot(w), v.dot(w)
    if aa == 0 and cc == 0:
        return (p-a).length_squared
    if aa == 0:
        s, t = 0., max(0., min(1., ee/cc))
    elif cc == 0:
        s, t = max(0., min(1., -dd/aa)), 0.
    else:
        denominator = aa*cc-bb*bb
        s = max(0., min(1., (bb*ee-cc*dd)/denominator)) if denominator > 1e-20*aa*cc else 0.
        t = (bb*s+ee)/cc
        if t < 0:
            t, s = 0., max(0., min(1., -dd/aa))
        elif t > 1:
            t, s = 1., max(0., min(1., (bb-dd)/aa))
    return (w+s*u-t*v).length_squared


def triangle_distance(a, b):
    minimum = min((p-geometry.closest_point_on_tri(p, *b)).length_squared for p in a)
    minimum = min(minimum, *( (p-geometry.closest_point_on_tri(p, *a)).length_squared for p in b))
    for first, other in ((a, b), (b, a)):
        for i in range(3):
            origin, delta = first[i], first[(i+1) % 3]-first[i]
            intersection = geometry.intersect_ray_tri(*other, delta, origin, True)
            if intersection is not None:
                parameter = (intersection-origin).dot(delta)/delta.length_squared
                if 0 <= parameter <= 1:
                    return 0.
    for i in range(3):
        for j in range(3):
            minimum = min(minimum, segment_distance_squared(a[i], a[(i+1) % 3], b[j], b[(j+1) % 3]))
    return math.sqrt(max(0., minimum))


def bounds(triangle):
    return tuple(min(p[k] for p in triangle) for k in range(3)), tuple(max(p[k] for p in triangle) for k in range(3))


class SolidParity:
    def __init__(self, points, ids):
        # Blender's float32 BVH may hit the same face twice when advancing a ray.
        # Count all intersections directly in float64 without advancing the origin.
        triangles = np.asarray(points, dtype=np.float64)[np.asarray(ids)]
        origin = triangles[:, 0]
        first, second = triangles[:, 1]-origin, triangles[:, 2]-origin
        normal = np.cross(first, second)
        area = np.linalg.norm(normal, axis=1)
        self.rays = []
        for xyz in ((.347, .511, .789), (-.673, .413, .611), (.229, -.917, .327),
                    (.733, .217, -.523), (-.197, -.671, .719), (.419, -.347, -.839)):
            direction = np.asarray(xyz, dtype=np.float64)
            direction /= np.linalg.norm(direction)
            cross = np.cross(direction, second)
            determinant = np.einsum('ij,ij->i', first, cross)
            valid = np.abs(determinant) > area*1e-12
            inverse = np.zeros(len(determinant))
            inverse[valid] = 1/determinant[valid]
            coefficients = [cross*inverse[:, None], np.cross(first, direction)*inverse[:, None], normal*inverse[:, None]]
            offsets = [np.einsum('ij,ij->i', origin, c) for c in coefficients]
            self.rays.append((valid, coefficients, offsets))

    def outside(self, point):
        point = np.asarray(point, dtype=np.float64)
        parities = []
        for valid, coefficients, offsets in self.rays:
            u, v, distance = [np.einsum('ij,j->i', c, point)-offset for c, offset in zip(coefficients, offsets)]
            forward = valid & (distance > 1e-10)
            candidate = forward & (u >= -1e-9) & (v >= -1e-9) & (u+v <= 1+1e-9)
            boundary = (np.abs(u) < 1e-9) | (np.abs(v) < 1e-9) | (np.abs(1-u-v) < 1e-9)
            if np.any(candidate & boundary):
                continue  # Choose another direction when a ray meets a shared edge.
            hits = forward & (u > 0) & (v > 0) & (u+v < 1)
            parities.append(int(np.count_nonzero(hits)) % 2)
            if len(parities) == 3:
                break
        assert len(parities) == 3 and len(set(parities)) == 1, ('Ambiguous solid parity', tuple(point), parities)
        return parities[0] == 0


def verify_clearance(body_points, body_ids, garment_points, garment_ids, clearance, tolerance):
    body = [tuple(body_points[i] for i in ids) for ids in body_ids]
    garment = [tuple(garment_points[i] for i in ids) for ids in garment_ids]
    tree = BVHTree.FromPolygons(body_points, body_ids, all_triangles=True)
    parity = SolidParity(body_points, body_ids)
    visible = {i for face in garment_ids for i in face}
    minimum_vertex = math.inf
    for i in visible:
        point = garment_points[i]
        nearest, _, _, distance = tree.find_nearest(point)
        assert nearest is not None and parity.outside(point), ('Inside garment point', i)
        minimum_vertex = min(minimum_vertex, distance)
    threshold = clearance-tolerance-2e-6  # FBX/GLB and Blender float32 allowance.
    assert minimum_vertex >= threshold, ('Vertex clearance', minimum_vertex, threshold)
    low, high = bounds(body_points)
    cell = max((Vector(high)-Vector(low)).length/80, clearance*4)
    bins = defaultdict(list)
    body_bounds = [bounds(t) for t in body]
    def cells(low, high):
        return product(*(range(math.floor(low[k]/cell), math.floor(high[k]/cell)+1) for k in range(3)))
    for index, (low, high) in enumerate(body_bounds):
        for key in cells(low, high):
            bins[key].append(index)
    minimum_triangle, pairs = clearance, 0
    for triangle in garment:
        low, high = bounds(triangle)
        candidates = set()
        for key in cells(tuple(v-clearance for v in low), tuple(v+clearance for v in high)):
            candidates.update(bins.get(key, ()))
        for index in candidates:
            other_low, other_high = body_bounds[index]
            squared = sum(max(low[k]-other_high[k], other_low[k]-high[k], 0.)**2 for k in range(3))
            if squared > clearance*clearance:
                continue
            pairs += 1
            distance = triangle_distance(triangle, body[index])
            minimum_triangle = min(minimum_triangle, distance)
            assert distance >= threshold, ('Triangle clearance', distance, threshold, tuple(tuple(p) for p in triangle), index)
    assert pairs > 0
    return {'minimumVertexClearanceMetres': minimum_vertex, 'minimumTriangleClearanceMetres': minimum_triangle,
            'trianglePairsTested': pairs, 'solidParityVertices': len(visible), 'bodyTriangles': len(body),
            'garmentTriangles': len(garment), 'formatAllowanceMetres': 2e-6}


results = []
for shape in ('adult', 'child'):
    folder = directory/shape
    reference = json.loads((folder/'FittedHumanReference.json').read_text())
    for extension, point_key in (('glb', 'renderPoints'), ('fbx', 'sourcePoints')):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        path = folder/('FittedHuman.'+extension)
        imported = bpy.ops.import_scene.gltf(filepath=str(path)) if extension == 'glb' else bpy.ops.import_scene.fbx(filepath=str(path))
        assert imported == {'FINISHED'}
        meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH' and any(m.type == 'ARMATURE' for m in o.modifiers)]
        rigs = [o for o in bpy.context.scene.objects if o.type == 'ARMATURE']
        assert len(meshes) == 5 and len(rigs) == 1 and len(rigs[0].data.bones) == 163
        errors, morph_counts = {}, {}
        for expected in reference['meshes']:
            obj = next(o for o in meshes if o.name == expected['name'])
            blocks = obj.data.shape_keys.key_blocks
            assert len(blocks)-1 == expected['morphCount'], (shape, extension, obj.name, len(blocks))
            for block in blocks:
                block.value = 0
            actual, _ = evaluated_geometry(obj)
            assert len(actual) == len(expected[point_key]), (obj.name, len(actual), len(expected[point_key]))
            error = max((a-convert(b)).length for a, b in zip(actual, expected[point_key]))
            assert error < 2e-6, (shape, extension, obj.name, error)
            errors[obj.name], morph_counts[obj.name] = error, len(blocks)-1
        body = next(o for o in meshes if o.name == 'OBJ')
        shirt = next(o for o in meshes if o.name == 'FittedShirt')
        assert 'GarmentDetail' in shirt.data.shape_keys.key_blocks
        body_points, body_triangles = evaluated_geometry(body)
        shirt_points, shirt_triangles = evaluated_geometry(shirt)
        clearance = verify_clearance(body_points, body_triangles, shirt_points, shirt_triangles,
                                     reference['clearance'], reference['tolerance'])
        visible = {i for triangle in shirt_triangles for i in triangle}
        for i in visible:
            assert abs(sum(w.weight for w in shirt.data.vertices[i].groups)-1) < 2e-5
        bone = rigs[0].pose.bones['spine02']
        bone.rotation_mode = 'QUATERNION'
        original = bone.rotation_quaternion.copy()
        bone.rotation_quaternion = original @ Quaternion((0, 0, 1), .25)
        bpy.context.view_layer.update()
        posed, _ = evaluated_geometry(shirt)
        movement = max((posed[i]-shirt_points[i]).length for i in visible)
        assert movement > .01 and all(math.isfinite(c) for i in visible for c in posed[i])
        bone.rotation_quaternion = original
        bpy.context.view_layer.update()
        bpy.ops.wm.save_as_mainfile(filepath=str(folder/('FittedHuman-'+extension+'.blend')))
        result = {'shape': shape, 'format': extension, 'inputSha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                  'meshes': 5, 'bones': 163, 'neutralPositionErrorsMetres': errors, 'morphCounts': morph_counts,
                  'visibleSkinMovementMetres': movement, 'neutralClearance': clearance,
                  'posedClearanceVerified': False}
        results.append(result)
        print(json.dumps(result), flush=True)
report = {'blender': bpy.app.version_string, 'results': results}
(directory/'BlenderSurfaceFittingVerification.json').write_text(json.dumps(report, indent=2)+'\n')
print(json.dumps(report))
