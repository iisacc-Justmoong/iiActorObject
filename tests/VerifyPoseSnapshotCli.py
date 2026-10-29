"""Check aliases and rejected arguments on a disposable copy, never the frozen source."""
from pathlib import Path
import hashlib,json,os,shutil,subprocess,sys,tempfile
program,source,destination=map(Path,sys.argv[1:]);destination.mkdir(parents=True,exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
original=sha(source);results=[]
with tempfile.TemporaryDirectory(prefix='cli-',dir=destination) as temporary:
    folder=Path(temporary);input_path=folder/'Input with spaces.fbx';shutil.copy2(source,input_path);assert sha(input_path)==original
    hard=folder/'Hard alias.fbx';os.link(input_path,hard);symbolic=folder/'Symbolic alias.fbx';symbolic.symlink_to(input_path.name)
    sentinel=folder/'Keep existing output.fbx';sentinel.write_bytes(b'existing output must survive rejected arguments\n');sentinel_hash=sha(sentinel)
    cases=[('same_path',[str(input_path),str(input_path)],1),('hard_link',[str(input_path),str(hard)],1),('symbolic_link',[str(input_path),str(symbolic)],1),
        ('nonfinite',[str(input_path),str(sentinel),'upperarm01.L','lowerarm01.L','nan','0','0'],1),
        ('unknown_joint',[str(input_path),str(sentinel),'__missing_joint__','lowerarm01.L','1','0','0'],1),
        ('usage',[str(input_path),str(sentinel),'incomplete'],2),
        ('separate_output',[str(input_path),str(folder/'Snapshot.fbx')],0)]
    for label,args,expected in cases:
        result=subprocess.run([str(program.resolve()),*args],capture_output=True,text=True)
        assert result.returncode==expected,(label,result.returncode,result.stdout,result.stderr)
        assert sha(input_path)==original and sha(source)==original,(label,'Input changed')
        assert sha(sentinel)==sentinel_hash,(label,'Rejected arguments changed existing output')
        results.append(dict(case=label,exitCode=result.returncode,sourceUnchanged=True))
    assert (folder/'Snapshot.fbx').stat().st_size>1000
report=dict(status='passed',cases=results,inputSha256=original,sourceUnchanged=True)
(destination/'Verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
