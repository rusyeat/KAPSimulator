# KAP character rework

`KAP_Characters_Rework.blend` contains the five editable variants and a review scene.
The FBX files share the project's `SK_Mannequin` skeleton and existing unarmed animations.
AdultMale and AdultFemale are shared by the existing NPC roles; equipment remains attached by the runtime code.
Boy, Girl and SickBoy are used by Boy, Girl and SickChild NPC kinds respectively.
Children use actor scale 0.67; their FBX files retain the adult skeleton scale for animation compatibility.

Each base mesh has approximately 14,000 triangles and one material slot.
Unreal imports provide three LODs, each with one rendering section.
UV channel 0 is the surface mapping; channel 1 encodes the material region in U.
Do not replace the region UV with an automatic unwrap. The shared `M_KAP_Human3`
material uses this channel for clothing, skin, eyes, hair and gloves.

Reimport through Unreal's Python console:

    py "D:/KAPSimulator/Scripts/import_character_assets_v3.py"

The importer writes only `/Game/KAP/Characters/V3`, assigns the existing physics
asset and skeleton, and does not replace the old character assets or skeleton pose.
Custom head and clothing geometry reuse the project's existing mannequin rig and fingers.
