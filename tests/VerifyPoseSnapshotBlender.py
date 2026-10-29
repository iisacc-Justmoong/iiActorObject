"""Actual FBX import: source points, all shape keys, skin weights, bone names/parents."""
from pathlib import Path
import hashlib,json,math,sys
import bpy
from mathutils import Vector

arguments=sys.argv[sys.argv.index('--')+1:];directory=Path(arguments[0]);expected_meshes=int(arguments[1]) if len(arguments)>1 else 5;assert expected_meshes in (4,5);results=[]
convert=lambda p:Vector((p[0],-p[2],p[1]))
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for shape in ['adult','child']:
    folder=directory/'human'/shape;path=folder/'PoseSnapshot.fbx';reference=folder/'Expected.json'
    expected=json.loads(reference.read_text());bpy.ops.wm.read_factory_settings(use_empty=True)
    assert bpy.ops.import_scene.fbx(filepath=str(path))=={'FINISHED'}
    armatures=[o for o in bpy.context.scene.objects if o.type=='ARMATURE'];assert len(armatures)==1
    bones=armatures[0].data.bones;assert len(bones)==len(expected['bones'])==163
    for bone in expected['bones']:
        actual=bones[bone['name']];assert (actual.parent.name if actual.parent else None)==bone['parent']
    objects={o.name:o for o in bpy.context.scene.objects if o.type=='MESH'};assert len(objects)==len(expected['meshes'])==expected_meshes
    maximum_point_error=maximum_weight_error=0.;points=channels=0
    for mesh in expected['meshes']:
        obj=objects[mesh['name']];assert len(obj.data.vertices)==len(mesh['weights'])
        assert len(obj.data.polygons)==mesh['polygons']
        actual_keys=obj.data.shape_keys.key_blocks if obj.data.shape_keys else {}
        assert set(actual_keys.keys())=={'Basis',*mesh['channels']},(shape,obj.name,len(actual_keys),len(mesh['channels']))
        points+=len(obj.data.vertices);channels+=len(mesh['channels'])
        for p,influences in enumerate(mesh['weights']):
            actual={obj.vertex_groups[g.group].name:g.weight for g in obj.data.vertices[p].groups if g.weight>0}
            target={expected['bones'][bone]['name']:weight for bone,weight in influences}
            assert actual.keys()==target.keys(),(shape,obj.name,p,actual,target)
            for name,weight in target.items():maximum_weight_error=max(maximum_weight_error,abs(actual[name]-weight))
        for case in mesh['cases']:
            for i,name in enumerate(mesh['channels']):actual_keys[name].value=case['pattern'][i%2]
            bpy.context.view_layer.update();evaluated=obj.evaluated_get(bpy.context.evaluated_depsgraph_get());geometry=evaluated.to_mesh()
            try:
                assert len(geometry.vertices)==len(case['points'])
                for vertex,target in zip(geometry.vertices,case['points']):
                    error=(evaluated.matrix_world@vertex.co-convert(target)).length
                    assert math.isfinite(error);maximum_point_error=max(maximum_point_error,error)
            finally:evaluated.to_mesh_clear()
        for name in mesh['channels']:actual_keys[name].value=0
    assert maximum_weight_error<1e-6,(shape,maximum_weight_error)
    assert maximum_point_error<2e-5,(shape,maximum_point_error)
    results.append(dict(shape=shape,inputSha256=sha(path),referenceSha256=sha(reference),bones=len(bones),meshes=len(objects),sourcePoints=points,shapeKeys=channels,morphWeightCases=3,maximumPointErrorMetres=maximum_point_error,maximumSkinWeightError=maximum_weight_error))
report=dict(status='passed',blenderVersion=bpy.app.version_string,results=results,morphNormalsVerified=False,actualCc5Verified=False,actualUnrealVerified=False,TposeCalibrationVerified=False)
(directory/'BlenderVerification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
