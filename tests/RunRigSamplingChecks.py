from pathlib import Path
import concurrent.futures,datetime,hashlib,json,os,re,subprocess,threading,xml.etree.ElementTree as ET

root=Path.cwd();out=root/'build/rig-sampling';lock=threading.RLock();failed=threading.Event();cache={};resource_cache={}
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def fingerprint():
    files=sorted({p for d in ['src','tests','example','cmake'] for p in (root/d).rglob('*') if p.is_file() and (p.suffix in ['.cpp','.h','.py','.cmake','.in'] or p.name=='CMakeLists.txt')}|{root/'CMakeLists.txt',root/'iiActorObject.h'})
    parts=[]
    for p in files:
        stat=p.stat();key=(stat.st_size,stat.st_mtime_ns,stat.st_ino);old=cache.get(p)
        if old is None or old[0]!=key:cache[p]=(key,sha(p))
        parts.append(str(p.relative_to(root))+':'+cache[p][1]+'\n')
    return hashlib.sha256(''.join(parts).encode()).hexdigest()
initial=fingerprint();frozen=[{'path':'resources/rig-sampling/adult.fbx','sha256':'12db89354a9fd2ffd4abc1cd2c88b7db104f567ff69cf1d8b9a75d68e2eb5c57'},{'path':'resources/rig-sampling/child.fbx','sha256':'003875cec34bc8f9790800658a8b12ad0635923656501b6eb3f9e20040951f4f'}]
run_id=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%f')
state={'status':'running','startedAt':datetime.datetime.now(datetime.timezone.utc).isoformat(),'sourceFingerprint':initial,
       'sourceEditsPermitted':False,'actualApplicationValidation':False,'steps':[],'active':{},'selectedTests':{}}
def save():
    state['updatedAt']=datetime.datetime.now(datetime.timezone.utc).isoformat();p=out/'MatrixState.tmp.json';p.write_text(json.dumps(state,ensure_ascii=False,indent=2)+'\n');p.replace(out/'MatrixState.json')
def unchanged():
    def verify(path,expected):
        stat=path.stat();signature=(stat.st_size,stat.st_mtime_ns,stat.st_ino)
        previous=resource_cache.get(path)
        if previous is None or previous[0]!=signature:
            digest=sha(path);assert digest==expected,str(path);resource_cache[path]=(signature,digest)
        else:assert previous[1]==expected,str(path)
    for v in frozen:verify(root/v['path'],v['sha256'])
    for name in ['MakeHuman','MakeHumanFace','MakeHumanMacro','MakeHumanExpressions']:
        directory=root/'resources'/name;manifestPath=directory/'UPSTREAM-SHA256.json'
        manifestHash=resource_cache.get(manifestPath)
        verify(manifestPath,manifestHash[1] if manifestHash else sha(manifestPath))
        manifest=json.loads(manifestPath.read_text())
        for item in manifest['files']:
            path=directory/item.get('local',item['path']);assert path.is_file(),str(path);verify(path,item['sha256'])
def run(label,args):
    with lock:
        if failed.is_set():raise RuntimeError(label+' cancelled because another phase failed')
        state['active'][label]=args;save()
    environment=os.environ.copy()
    if label.startswith('sanitizers-'):
        environment['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
        environment['ASAN_OPTIONS']='halt_on_error=1:abort_on_error=1'
    with (out/(label+'.log')).open('w') as log:
        process=subprocess.run(args,stdout=log,stderr=subprocess.STDOUT,cwd=root,env=environment)
    with lock:
        state['active'].pop(label,None);state['steps'].append({'label':label,'exitCode':process.returncode});save()
        if process.returncode:failed.set()
    if process.returncode:raise RuntimeError(label+':\n'+(out/(label+'.log')).read_text()[-12000:])
    print(label,'passed',flush=True)
def select(binary,label,full):
    tests=json.loads(subprocess.check_output(['ctest','--test-dir',binary,'-N','--show-only=json-v1'],text=True))['tests'];chosen=[]
    applications={'iiactorobjectrigsamples','iiactorobjectposesnapshot','iiactorobjectcore3d',
                  'iiactorobjectcharacterize','iiactorobjecttemplatecharacter','iiactorobjectcharacterauthoring','iiactorobjectdnacharacter'}
    def runsApplication(test):
        name=test['name'].lower()
        if name.endswith(('.cli','.project_cli','.example','.example_source','.native_example','.example_safety')):return True
        for argument in test.get('command',[]):
            base=argument.replace('\\','/').rsplit('/',1)[-1].lower().removesuffix('.exe')
            if base in applications:return True
        return False
    for test in tests:
        name=test['name'];command=' '.join(test.get('command',[])).lower()
        if runsApplication(test) or any(term in command for term in ('blender','unreal','charactercreator')) or '.blender' in name.lower():continue
        if not full and ('.adult' in name or '.child' in name or name=='iiActorObject.project.human'):continue
        selected=full or bool(re.match(r'^iiActorObject\.(rig_samples|rig_snapshot|deformation_snapshot|pose_snapshot|reference_pose|pose_aim|pose_frame|pose_rig|rig|project|fbx_import)\.',name)) or name in ['iiActorObject.object3d.ownership','iiActorObject.layers.ownership','iiActorObject.template.ownership','iiActorObject.execution_policy.registration']
        if label=='sanitizers-tests':
            selected=((name.startswith('iiActorObject.rig_samples.') and not name.endswith(('.cli','.project_cli','.text_budget'))) or name in {
                'iiActorObject.rig_snapshot.controls','iiActorObject.rig_snapshot.partial','iiActorObject.rig_snapshot.dna',
                'iiActorObject.deformation_snapshot.topology','iiActorObject.deformation_snapshot.generated_normals',
                'iiActorObject.deformation_snapshot.reference','iiActorObject.deformation_snapshot.roundtrip',
                'iiActorObject.project.layers','iiActorObject.project.corruption','iiActorObject.project.atomic',
                'iiActorObject.project.restore_validation','iiActorObject.fbx_import.precision','iiActorObject.rig.evaluation',
                'iiActorObject.execution_policy.registration'})
        if selected:chosen.append(name)
    assert chosen
    args=['ctest','--test-dir',binary,'--parallel','2','--fixture-exclude-any','^iiActorObject_surface_fairing_human$',
          '-R','^('+'|'.join(re.escape(n) for n in chosen)+')$']
    plan=json.loads(subprocess.check_output(args+['-N','--show-only=json-v1'],text=True))['tests']
    for t in plan:
        text=(t['name']+' '+' '.join(t.get('command',[]))).lower()
        assert not runsApplication(t) and not any(w in text for w in ('blender','unreal','charactercreator')),t['name']
        assert t['name'] not in ['iiActorObject.surface_collision.human','iiActorObject.surface_fairing.human','iiActorObject.surface_fitting.human'],t['name']
    with lock:state['selectedTests'][label]=[v['name'] for v in plan];save()
    run(label,args+['--output-on-failure','--output-junit','rig-sampling-matrix.xml'])
def consumers(label,binary,prefix,source,destination,hasdna):
    run(label+'-install',['cmake','--install',binary,'--prefix',str(root/prefix)])
    run(label+'-consumer-configure',['cmake','-S',source,'-B',destination,'-DiiActorObject_DIR='+str(root/prefix/'lib/cmake/iiActorObject')]+(['-DIIACTOROBJECT_IMAGE_TEST_FIXTURE='+str(root/'resources/fixtures/images/checker.png')] if label in ['core','nodna'] else []))
    run(label+'-consumer-build',['cmake','--build',destination,'--parallel','2'])
    args=['ctest','--test-dir',destination,'--parallel','2','--output-on-failure','--output-junit','rig-sampling-consumer.xml']
    if label in ['shared','sanitizers']:args+=['-R','^iiActorObject\.(installed_consumer|installed_characterization_consumer|installed_pose_snapshot_consumer)$']
    run(label+'-consumer-tests',args)
    dest=f'build/rig-sampling/consumer-{label}'
    cfg=['cmake','-S','tests/rig_samples_consumer','-B',dest,'-DiiActorObject_DIR='+str(root/prefix/'lib/cmake/iiActorObject')]
    if hasdna:cfg+=['-DIIACTOROBJECT_DNA_FIXTURE_DIRECTORY='+str(root/'build/dna')]
    if label=='sanitizers':cfg+=['-DCMAKE_BUILD_TYPE=Debug','-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer']
    run(label+'-new-consumer-configure',cfg);run(label+'-new-consumer-build',['cmake','--build',dest,'--parallel','2'])
    tests=['ctest','--test-dir',dest,'--parallel','2','--output-on-failure','--output-junit','result.xml']
    if label=='sanitizers':tests+=['-R','^installed\.rig_samples\.(endpoints|formats|dna)$']
    run(label+'-new-consumer-tests',tests)
def profile(label,binary,prefix,source,destination,hasdna,alltests):
    if label!='shared':
        run(label+'-configure',['cmake','-S','.','-B',binary,'-DIIACTOROBJECT_BLENDER_EXECUTABLE=',
            '-DIIACTOROBJECT_BUILD_TOOLS=ON','-DIIACTOROBJECT_VALIDATE_APPLICATION_EXECUTION=OFF'])
        command=['cmake','--build',binary]
        if label=='sanitizers':command+=['--target','iiActorObjectRigSamplesTests','iiActorObjectRigSamples',
            'iiActorObjectRigSnapshotTests','iiActorObjectDeformationSnapshotTests','iiActorObjectProjectTests',
            'iiActorObjectFbxImportPrecisionTests','iiActorObjectRigTests','iiActorObjectDnaFixture']
        run(label+'-build',command+['--parallel','2'])
        select(binary,label+'-tests',alltests)
    consumers(label,binary,Path(prefix),source,destination,hasdna)
profiles=[('shared','build','build/install','tests/consumer','build/consumer/build',True,False),
          ('core','build/core-only/build','build/core-only/install','tests/core_consumer','build/core-only/consumer/build',True,True),
          ('nodna','build/no-dna/build','build/no-dna/install','tests/core_consumer','build/no-dna/consumer/build',False,True),
          ('sanitizers','build/sanitizers/build','build/sanitizers/install','tests/consumer','build/sanitizers/consumer/build',True,False)]
save()
try:
    unchanged();run('shared-configure',['cmake','-S','.','-B','build','-DIIACTOROBJECT_BLENDER_EXECUTABLE=',
        '-DIIACTOROBJECT_VALIDATE_APPLICATION_EXECUTION=OFF']);run('shared-build',['cmake','--build','build','--parallel','2']);select('build','shared-tests',False)
    run('resource-reference-template',[str(root/'build/iiActorObjectHumanReferenceTemplateTests'),
        str(root/'resources/MakeHuman'),str(root/'resources/MakeHumanFace'),str(root/'resources/MakeHumanMacro'),
        str(root/'resources/MakeHumanExpressions'),str(out/('resource-reference-'+run_id))])
    for shape in ['adult','child']:
        run('human-'+shape,[str(root/'build/iiActorObjectRigSamplesTests'),'human',str(root/'resources/rig-sampling'/(shape+'.fbx'))])
    errors=[]
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        for future in concurrent.futures.as_completed([pool.submit(profile,*p) for p in profiles]):
            try:future.result()
            except Exception as e:failed.set();errors.append(str(e))
    if errors:raise RuntimeError('\n'.join(errors))
    unchanged();assert fingerprint()==initial;state['counts']={}
    for label,binary,_,_,destination,_,_ in profiles:
        for kind,folder,name in [('native',binary,'rig-sampling-matrix.xml'),('consumer',destination,'rig-sampling-consumer.xml'),('new-consumer',f'build/rig-sampling/consumer-{label}','result.xml')]:
            suite=ET.parse(root/folder/name).getroot();counts={k:int(suite.get(k,'0')) for k in ('tests','failures','skipped')}
            assert counts['tests']>0 and not counts['failures'] and not counts['skipped'],counts;state['counts'][label+'-'+kind]=counts
    state['status']='passed'
except Exception as e:
    state['status']='failed';state['error']=str(e);raise
finally:
    state['sourceEditsPermitted']=True;save();print(json.dumps({'status':state['status'],'counts':state.get('counts'),'error':state.get('error')}),flush=True)
