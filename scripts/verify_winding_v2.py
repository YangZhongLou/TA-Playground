import math

R = 200.0
H = 400.0
HalfH = H / 2

BL = (200.0, 0.0, -HalfH)
BR = (100.0, 173.2, -HalfH)
TR = (100.0, 173.2, HalfH)
TL = (200.0, 0.0, HalfH)

def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
def sub(a, b):
    return (a[0]-b[0], a[1]-b[1], a[2]-b[2])
def area2d(a, b, c):
    return a[0]*(b[1]-c[1]) + b[0]*(c[1]-a[1]) + c[0]*(a[1]-b[1])

# Build face-local 2D coordinates
# Face tangent (along edge BL->BR)
normal = (math.cos(math.radians(30)), math.sin(math.radians(30)), 0)
tangent = (-normal[1], normal[0], 0)  # perpendicular to normal in XY plane

def proj(v):
    dx, dy, dz = v[0]-BL[0], v[1]-BL[1], v[2]-BL[2]
    return (dx*tangent[0]+dy*tangent[1], dz)

bl = proj(BL)  # (0, 0)
br = proj(BR)  # (200, 0)
tr = proj(TR)  # (200, 400)
tl = proj(TL)  # (0, 400)

print("Face-local 2D coords:")
print(f"  BL = {bl}")
print(f"  BR = {br}")
print(f"  TR = {tr}")
print(f"  TL = {tl}")

print("\n=== STANDARD MATH (Y up): CCW = positive area ===")
a_orig = area2d(bl, tl, tr)
a_fix  = area2d(bl, br, tr)
print(f"Original BL->TL->TR: area = {a_orig:8.1f} -> {'CCW' if a_orig > 0 else 'CW'}")
print(f"Fixed   BL->BR->TR: area = {a_fix:8.1f} -> {'CCW' if a_fix > 0 else 'CW'}")

print("\n=== UE SCREEN SPACE (Y down): CW = positive area ===")
# In screen coords: Y is down, so flip Y
bl_s = (bl[0], -bl[1])
br_s = (br[0], -br[1])
tr_s = (tr[0], -tr[1])
tl_s = (tl[0], -tl[1])

a_orig_s = area2d(bl_s, tl_s, tr_s)
a_fix_s  = area2d(bl_s, br_s, tr_s)
print(f"Original BL->TL->TR: area = {a_orig_s:8.1f} -> {'CW (FRONT in UE)' if a_orig_s > 0 else 'CCW (BACK in UE)'}")
print(f"Fixed   BL->BR->TR: area = {a_fix_s:8.1f} -> {'CW (FRONT in UE)' if a_fix_s > 0 else 'CCW (BACK in UE)'}")

print("\n=== CROSS PRODUCT (actual 3D normal direction) ===")
E1 = sub(TL, BL)
E2 = sub(TR, BL)
N_orig = cross(E1, E2)
print(f"Original BL->TL->TR: Cross = ({N_orig[0]:.1f}, {N_orig[1]:.1f})")

E1 = sub(BR, BL)
E2 = sub(TR, BL)
N_fix = cross(E1, E2)
print(f"Fixed   BL->BR->TR: Cross = ({N_fix[0]:.1f}, {N_fix[1]:.1f})")

outward = normal
dot_orig = N_orig[0]*outward[0] + N_orig[1]*outward[1]
dot_fix  = N_fix[0]*outward[0] + N_fix[1]*outward[1]
print(f"\nDot with outward normal:")
print(f"  Original: {dot_orig:.1f} -> {'OUTWARD' if dot_orig > 0 else 'INWARD'}")
print(f"  Fixed:    {dot_fix:.1f} -> {'OUTWARD' if dot_fix > 0 else 'INWARD'}")

print("\n=== CORRECTED CONCLUSION ===")
print("UE/DirectX screen space: Y is DOWN")
print("  In screen coords: positive area = CW = FRONT face (visible)")
print("")
if a_orig_s > 0:
    print("  ORIGINAL code: CW on screen = FRONT face = VISIBLE")
else:
    print("  ORIGINAL code: CCW on screen = BACK face = CULLED")

if a_fix_s > 0:
    print("  FIXED code:    CW on screen = FRONT face = VISIBLE")
else:
    print("  FIXED code:    CCW on screen = BACK face = CULLED")
