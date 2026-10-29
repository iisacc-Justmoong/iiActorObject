#include <iiActorObject/MeshGeometry.h>
#include <iiActorObject/MeshTopology.h>
#include <type_traits>

// Both previous public headers retain their skeletal declarations.
static_assert(std::is_class_v<iiActorObject::SkeletalMesh>);
static_assert(std::is_base_of_v<iiActorObject::SceneComponent,iiActorObject::SkeletalMeshComponent>);

int boneIndexHeaderProbe();
int meshDataHeaderProbe();
int meshTopologyCoreHeaderProbe();
int meshGeometryCoreHeaderProbe();
int surfaceBindingCoreHeaderProbe();
int surfaceAttachmentHeaderProbe();
int surfaceCompatibilityHeaderProbe();
int object3DDataHeaderProbe();
int object3DMaterialHeaderProbe();
int object3DMeshHeaderProbe();
int object3DAssetCompatibilityHeaderProbe();
int meshSelectionCoreHeaderProbe();
int meshSelectionAssetHeaderProbe();
int meshSelectionCompatibilityHeaderProbe();
int skeletalMeshCoreHeaderProbe();
int skeletalMeshComponentHeaderProbe();
int main()
{
    return boneIndexHeaderProbe() || meshDataHeaderProbe() ||
           meshTopologyCoreHeaderProbe() || meshGeometryCoreHeaderProbe() ||
           surfaceBindingCoreHeaderProbe() || surfaceAttachmentHeaderProbe() ||
           surfaceCompatibilityHeaderProbe() || object3DDataHeaderProbe() || object3DMaterialHeaderProbe() || object3DMeshHeaderProbe() ||
           object3DAssetCompatibilityHeaderProbe() || meshSelectionCoreHeaderProbe() ||
           meshSelectionAssetHeaderProbe() || meshSelectionCompatibilityHeaderProbe() ||
           skeletalMeshCoreHeaderProbe() || skeletalMeshComponentHeaderProbe();
}
