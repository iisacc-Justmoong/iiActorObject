"""CC5 본 대응의 키가 실제 Blender FBX 본과 일치하는지 검사한다."""
from pathlib import Path
import configparser
import hashlib
import json
import sys
import bpy

arguments=sys.argv[sys.argv.index('--')+1:]
destination=Path(arguments[0])
inputs=arguments[1:]
assert inputs and len(inputs)%3==0
root=Path(__file__).resolve().parents[1]
required={'Hips','Spine','Head','LeftArm','LeftForeArm','LeftHand','RightArm','RightForeArm','RightHand',
          'LeftUpLeg','LeftLeg','LeftFoot','RightUpLeg','RightLeg','RightFoot'}
parents={'Spine':'Hips','Head':'Spine','LeftArm':'Spine','LeftForeArm':'LeftArm','LeftHand':'LeftForeArm',
         'RightArm':'Spine','RightForeArm':'RightArm','RightHand':'RightForeArm','LeftUpLeg':'Hips',
         'LeftLeg':'LeftUpLeg','LeftFoot':'LeftLeg','RightUpLeg':'Hips','RightLeg':'RightUpLeg','RightFoot':'RightLeg'}
sha=lambda path:hashlib.sha256(path.read_bytes()).hexdigest()
def relative(path):
    try:return str(path.relative_to(root))
    except ValueError:return str(path)
results=[]
for offset in range(0,len(inputs),3):
    shape,source,profile=inputs[offset:offset+3]
    source,profile=Path(source),Path(profile)
    raw=profile.read_bytes()
    assert raw.startswith(b'\xef\xbb\xbf') and b'\n' not in raw.replace(b'\r\n',b'')
    parser=configparser.ConfigParser(interpolation=None,delimiters=('=',),strict=True)
    parser.optionxform=str
    parser.read_string(raw.decode('utf-8-sig'))
    assert parser.sections()==['BoneMap']
    mapping=dict(parser['BoneMap'])
    assert len(mapping)==53 and len(set(mapping.values()))==53
    by_role={role:bone for bone,role in mapping.items()}
    assert required<=by_role.keys() and by_role['Hips']=='root' and by_role['Spine']=='spine05'
    assert 'LeftToeBase' not in by_role and 'RightToeBase' not in by_role
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(source))
    armatures=[obj for obj in bpy.context.scene.objects if obj.type=='ARMATURE']
    assert len(armatures)==1
    bones=armatures[0].data.bones
    assert len(bones)==163 and all(name in bones for name in mapping)
    for child,parent in parents.items():
        cursor=bones[by_role[child]].parent
        while cursor and cursor.name!=by_role[parent]:cursor=cursor.parent
        assert cursor is not None,(shape,child,parent)
    results.append(dict(shape=shape,input=relative(source),inputSha256=sha(source),profile=relative(profile),
        profileSha256=sha(profile),mappedRoles=len(mapping),actualBlenderBones=len(bones),
        requiredBodyRoles=len(required),hierarchyVerified=True))
report=dict(blenderVersion=bpy.app.version_string,results=results,actualBlenderBoneNamesVerified=True,
    actualCc5CharacterizationVerified=False,TposeVerified=False)
destination.parent.mkdir(parents=True,exist_ok=True)
destination.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report))
