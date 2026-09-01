"""
Mode A: attack_Skeleton (Root=根节点, scale=1)
  Root.Y = -Root.Z  (Z位移→Y, 取反)
  Root.Z = 0
  Bip001.Z = 0
"""
import unreal

Q_X90 = unreal.Quat(0.70710678, 0.0, 0.0, 0.70710678)
V_ONE = unreal.Vector(1.0, 1.0, 1.0)


def fix_animation(anim_path):
    seq = unreal.load_asset(anim_path)
    if not seq:
        return False
    ctrl = seq.controller
    al = unreal.AnimationLibrary
    nf = al.get_num_frames(seq)

    # 读取 Root 全部帧
    root_z = []
    for f in range(nf):
        root_z.append(al.get_bone_pose_for_frame(seq, 'Root', f, False).translation.z)

    # 读取 Bip001 全部帧
    bip_pos, bip_rot, bip_scl = [], [], []
    for f in range(nf):
        tr = al.get_bone_pose_for_frame(seq, 'Bip001', f, False)
        bip_pos.append(tr.translation)
        bip_rot.append(tr.rotation)
        bip_scl.append(tr.scale3d)

    print(f'{anim_path}: Root.Z [{min(root_z):.2f}..{max(root_z):.2f}], {nf}f')

    # Root: Y = -Z, Z=0, roll=90°, scale=1
    ctrl.set_bone_track_keys('Root',
        [unreal.Vector(0, -z, 0) for z in root_z],
        [Q_X90] * nf,
        [V_ONE] * nf)

    # Bip001: Z=0
    ctrl.set_bone_track_keys('Bip001',
        [unreal.Vector(p.x, p.y, 0) for p in bip_pos],
        bip_rot, bip_scl)

    # 不修改 RootMotion 设置, 保留导入状态
    unreal.EditorAssetLibrary.save_asset(seq.get_path_name())
    return True


ANIM_LIST = [
    '/Game/Asset/珂蕾妲/test01/K_Attack_01',
]

if __name__ == '__main__':
    ok = fail = 0
    for p in ANIM_LIST:
        if fix_animation(p): ok += 1
        else: fail += 1
    print(f'{ok} OK, {fail} failed')
