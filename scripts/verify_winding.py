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

outward = (math.cos(math.radians(30)), math.sin(math.radians(30)), 0)
print('Outward normal:', outward)

# ORIGINAL: BL->TL->TR
E1 = sub(TL, BL)
E2 = sub(TR, BL)
N = cross(E1, E2)
dot = N[0]*outward[0] + N[1]*outward[1]
print('\nORIGINAL BL->TL->TR:')
print(f'  Cross = ({N[0]:.1f}, {N[1]:.1f})')
dir_str = 'OUTWARD' if dot > 0 else 'INWARD'
print(f'  Dot with outward = {dot:.1f} ({dir_str})')

# Project to face plane for winding check
normal = outward
tangent = (-normal[1], normal[0], 0)
def proj(v):
    dx, dy, dz = v[0]-BL[0], v[1]-BL[1], v[2]-BL[2]
    return (dx*tangent[0]+dy*tangent[1], dz)

bl = proj(BL)
tl = proj(TL)
tr = proj(TR)

# Standard math (Y up): CCW = positive
a = area2d(bl, tl, tr)
print(f'  Area (std math) = {a:.1f} ({"CCW" if a > 0 else "CW"})')

# Screen coords (Y down, UE/DirectX): CW = positive (front face)
bl_s = (bl[0], -bl[1])
tl_s = (tl[0], -tl[1])
tr_s = (tr[0], -tr[1])
a_s = area2d(bl_s, tl_s, tr_s)
face_str = 'CW=FRONT' if a_s > 0 else 'CCW=BACK'
print(f'  Area (screen Y-down) = {a_s:.1f} ({face_str})')

# CURRENT FIX: BL->BR->TR
print('\nFIXED BL->BR->TR:')
E1 = sub(BR, BL)
E2 = sub(TR, BL)
N = cross(E1, E2)
dot = N[0]*outward[0] + N[1]*outward[1]
print(f'  Cross = ({N[0]:.1f}, {N[1]:.1f})')
dir_str = 'OUTWARD' if dot > 0 else 'INWARD'
print(f'  Dot with outward = {dot:.1f} ({dir_str})')

br = proj(BR)
a = area2d(bl, br, tr)
print(f'  Area (std math) = {a:.1f} ({"CCW" if a > 0 else "CW"})')
br_s = (br[0], -br[1])
tr_s = (tr[0], -tr[1])
a_s = area2d(bl_s, br_s, tr_s)
face_str = 'CW=FRONT' if a_s > 0 else 'CCW=BACK'
print(f'  Area (screen Y-down) = {a_s:.1f} ({face_str})')

print('\n=== KEY INSIGHT ===')
print('In standard 3D math (Y up):')
print('  Original BL->TL->TR is CW from outside -> cross points INWARD')
print('  Fixed   BL->BR->TR is CCW from outside -> cross points OUTWARD')
print('')
print('BUT UE uses screen coords where Y is DOWN!')
print('  Original BL->TL->TR projects to CW on screen = FRONT face in UE')
print('  Fixed   BL->BR->TR projects to CCW on screen = BACK face in UE')
print('')
print('So ORIGINAL code was actually CORRECT for UE/DirectX!')
print('My fix made it WORSE (reversed the winding for UE)!')
