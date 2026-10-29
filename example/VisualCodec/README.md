# iiActorObject and iiVisualCodec composition

This independently configured application adapter connects the FBX image
resolver interface to iiVisualCodec. The actor SDK and codec are selected by
this consumer, and the adapter is installed as the separate
`iiActorObjectVisualCodec` package with target `iiActorObject::VisualCodec`.

The original optional adapter remains in the actor module while this separate
project is being verified. Do not enable both adapters in the same application.
Inputs for the synthetic FBX and image contracts remain in the parent module's
`resources/fixtures`; generated build outputs stay in the parent `build/`.
See `docs/Boundary.md` for migration and verification details.
