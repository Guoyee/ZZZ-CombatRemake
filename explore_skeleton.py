import unreal

sk_path = '/Game/Asset/珂蕾妲/attacktest/K_Attack_01_Skeleton'
sk = unreal.load_asset(sk_path)

# Explore skeleton editing APIs
print('=== Skeleton API ===')
bone_methods = [m for m in dir(sk) if 'bone' in m.lower() or 'remove' in m.lower() or 'add' in m.lower() or 'edit' in m.lower() or 'reparent' in m.lower() or 'set_' in m.lower()]
for m in sorted(bone_methods):
    print(f'  {m}')

# Check for skeleton editing tools
print('\n=== SkeletonEditingTool ===')
try:
    import skeleton_editing
    print(dir(skeleton_editing))
except:
    print('  Not available')

# Check mesh API for skeleton modification
print('\n=== SkeletalMesh bone editing ===')
mesh = unreal.load_asset('/Game/Asset/珂蕾妲/attacktest/K_Attack_01')
mesh_methods = [m for m in dir(mesh) if 'bone' in m.lower() or 'skeleton' in m.lower() or 'ref' in m.lower()]
for m in sorted(mesh_methods):
    print(f'  {m}')

# Check if we can access the skeleton's bone tree for editing
print('\n=== Skeleton bone tree ===')
try:
    bt = sk.get_bone_tree()
    print(f'  get_bone_tree: {type(bt)}')
except Exception as e:
    print(f'  get_bone_tree: {e}')
try:
    bl = sk.get_blend_profile()
    print(f'  get_blend_profile: {type(bl)}')
except Exception as e:
    print(f'  get_blend_profile: {e}')

# Can we re-set the ref pose?
print('\n=== Skeleton ref pose access ===')
try:
    ref = sk.get_ref_bone_pose()
    print(f'  get_ref_bone_pose: {type(ref)}')
except Exception as e:
    print(f'  get_ref_bone_pose: {e}')
try:
    # Maybe we can use the reference pose info?
    rpi = sk.get_reference_pose_info()
    print(f'  get_reference_pose_info: {type(rpi)}')
except Exception as e:
    print(f'  get_reference_pose_info: {e}')

# Try AnimationBlueprintLibrary or similar
print('\n=== AnimationBlueprintLibrary ===')
try:
    abp_lib = unreal.AnimationBlueprintLibrary
    methods = [m for m in dir(abp_lib) if 'skeleton' in m.lower() or 'bone' in m.lower()]
    for m in sorted(methods):
        print(f'  {m}')
except Exception as e:
    print(f'  Error: {e}')

# Try RigHierarchy or similar
print('\n=== EditorAssetLibrary skeleton operations ===')
eal = unreal.EditorAssetLibrary
methods = [m for m in dir(eal) if 'skeleton' in m.lower() or 'bone' in m.lower() or 'rename' in m.lower()]
for m in sorted(methods):
    print(f'  {m}')
