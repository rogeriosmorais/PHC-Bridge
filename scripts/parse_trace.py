import csv
import sys
from collections import defaultdict

file_path = r"F:\NewEngine\PhysAnimUE5\Saved\PhysAnim\Traces\20260313-002954-Lvl_ThirdPerson-BP_PhysAnimCharacter_C_UAID_E865382E9A449FC602_1821632568\frames.csv"

max_ang = defaultdict(float)
max_lin = defaultdict(float)
max_off = defaultdict(float)
obs_mean = defaultdict(float)

with open(file_path, 'r', encoding='utf-8') as f:
    reader = csv.DictReader(f)
    for row in reader:
        phase = row.get('movement_smoke_phase', 'Unknown')
        
        ang = float(row.get('max_body_angular_speed_deg_per_second', 0))
        max_ang[phase] = max(max_ang[phase], ang)
        
        lin = float(row.get('max_body_linear_speed_cm_per_second', 0))
        max_lin[phase] = max(max_lin[phase], lin)
        
        obs = float(row.get('self_observation_mean_abs', 0))
        obs_mean[phase] = max(obs_mean[phase], obs)
        
for phase in max_ang.keys():
    print(f"{phase}: MaxAng={max_ang[phase]:.0f} deg/s, MaxLin={max_lin[phase]:.0f} cm/s, MaxObsMeanAbs={obs_mean[phase]:.3f}")
