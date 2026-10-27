import unreal
# Жануарлардың рагдолл физикасын қайта құру: SKM_KAP_{Horse,Cow,Goat,Sheep,Fox}_PhysicsAsset
# (әр сүйекке капсула, буын шектері — KAPAnimalPhysGen.inl). NullRHI командлетте іске қосылады.
ok = unreal.KAPEditorLibrary.build_animal_physics()
unreal.log('KAPANIMPHYS result: %s' % ok)
