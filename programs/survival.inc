; Shared bytecode survival gate.
; Call `CALL survival_check` before behaviour selection or demo loops.

survival_check:
    IN   A,(ENERGY_SOURCE)
    CP   ENERGY_CHARGER
    JP   Z,survival_keep_charging
    CP   ENERGY_SOLAR
    JP   Z,survival_keep_solar
    CP   ENERGY_JUNCTION
    JP   Z,survival_keep_junction

    IN   A,(BATTERY)
    CP   96
    JP   C,survival_recover

survival_recovered:
    LD   A,NAV_WANDER
    OUT  (NAV_INTENT),A
    RET

survival_keep_charging:
    IN   A,(BATTERY)
    CP   250
    JP   C,survival_charger
    JP   survival_recovered

survival_keep_solar:
    IN   A,(BATTERY)
    CP   160
    JP   C,survival_solar
    JP   survival_recovered

survival_keep_junction:
    IN   A,(BATTERY)
    CP   176
    JP   C,survival_junction
    JP   survival_recovered

survival_recover:
    CP   72
    JP   NC,survival_recover_strategy

    IN   A,(NAV_STATUS)
    CP   NAV_BLOCKED
    JP   Z,survival_blocked_recover

    JP   survival_emergency

survival_recover_strategy:
    IN   A,(STRATEGY_TENDENCY)
    CP   160
    JP   NC,survival_feral_recover

survival_house_recover:
    IN   A,(CHARGER_SLOTS)
    CP   1
    JP   C,survival_house_no_slot

    IN   A,(CHARGER_DISTANCE)
    CP   64
    JP   C,survival_charger

    IN   A,(SOLAR_LEVEL)
    CP   48
    JP   NC,survival_solar

    IN   A,(JUNCTION_DISTANCE)
    CP   48
    JP   C,survival_junction

    JP   survival_charger_wait

survival_house_no_slot:
    IN   A,(BATTERY)
    CP   72
    JP   C,survival_no_slot_critical

    IN   A,(CHARGER_DISTANCE)
    CP   56
    JP   C,survival_charger_wait

    IN   A,(SOLAR_LEVEL)
    CP   96
    JP   NC,survival_solar

    IN   A,(JUNCTION_DISTANCE)
    CP   72
    JP   C,survival_junction

    JP   survival_charger

survival_blocked_recover:
    IN   A,(SOLAR_LEVEL)
    CP   24
    JP   NC,survival_solar

    IN   A,(CHARGER_SLOTS)
    CP   1
    JP   C,survival_no_slot_critical

    IN   A,(CHARGER_DISTANCE)
    CP   64
    JP   C,survival_charger

    IN   A,(JUNCTION_DISTANCE)
    CP   32
    JP   C,survival_junction

    JP   survival_charger_wait

survival_feral_recover:
    IN   A,(JUNCTION_DISTANCE)
    CP   56
    JP   C,survival_junction

    IN   A,(SOLAR_LEVEL)
    CP   72
    JP   NC,survival_solar

    IN   A,(CHARGER_SLOTS)
    CP   1
    JP   NC,survival_charger

    IN   A,(JUNCTION_DISTANCE)
    CP   104
    JP   C,survival_junction

    IN   A,(SOLAR_LEVEL)
    CP   48
    JP   NC,survival_solar

    IN   A,(CHARGER_DISTANCE)
    CP   56
    JP   C,survival_charger_wait

    JP   survival_charger

survival_emergency:
    IN   A,(JUNCTION_DISTANCE)
    CP   40
    JP   C,survival_junction

    IN   A,(CHARGER_SLOTS)
    CP   1
    JP   C,survival_emergency_no_charger

    IN   A,(CHARGER_DISTANCE)
    CP   48
    JP   C,survival_charger

survival_emergency_no_charger:
    IN   A,(SOLAR_LEVEL)
    CP   32
    JP   NC,survival_solar

    IN   A,(JUNCTION_DISTANCE)
    CP   56
    JP   C,survival_junction

    IN   A,(CHARGER_SLOTS)
    CP   1
    JP   NC,survival_charger

survival_no_slot_critical:
    IN   A,(SOLAR_LEVEL)
    CP   32
    JP   NC,survival_solar

    IN   A,(CHARGER_DISTANCE)
    CP   56
    JP   C,survival_charger_wait

    JP   survival_charger

survival_charger_wait:
    LD   A,NAV_CHARGER
    OUT  (NAV_INTENT),A
    IN   A,(CHARGER_BEARING)
    OUT  (HEAD_YAW),A
    LD   A,SKILL_REST
    OUT  (SKILL),A
    LD   A,GAIT_LOAF
    OUT  (GAIT),A
    LD   A,EAR_SCAN
    OUT  (EAR_POSE),A
    LD   A,TAIL_WRAP
    OUT  (TAIL_POSE),A
    LD   A,VOCAL_PURR
    OUT  (VOCAL),A
    LD   A,156
    OUT  (EYELID),A
    JP   survival_check

survival_charger:
    LD   A,NAV_CHARGER
    OUT  (NAV_INTENT),A
    IN   A,(CHARGER_BEARING)
    OUT  (HEAD_YAW),A
    LD   A,SKILL_WALK
    OUT  (SKILL),A
    LD   A,GAIT_WALK
    OUT  (GAIT),A
    LD   A,EAR_SCAN
    OUT  (EAR_POSE),A
    LD   A,TAIL_FLAG
    OUT  (TAIL_POSE),A
    LD   A,VOCAL_SILENT
    OUT  (VOCAL),A
    LD   A,230
    OUT  (EYELID),A
    JP   survival_check

survival_solar:
    LD   A,NAV_SOLAR
    OUT  (NAV_INTENT),A
    LD   A,SKILL_REST
    OUT  (SKILL),A
    LD   A,GAIT_LOAF
    OUT  (GAIT),A
    LD   A,128
    OUT  (HEAD_YAW),A
    LD   A,EAR_SWIVEL
    OUT  (EAR_POSE),A
    LD   A,TAIL_WRAP
    OUT  (TAIL_POSE),A
    LD   A,VOCAL_SILENT
    OUT  (VOCAL),A
    LD   A,176
    OUT  (EYELID),A
    JP   survival_check

survival_junction:
    LD   A,NAV_JUNCTION
    OUT  (NAV_INTENT),A
    IN   A,(JUNCTION_BEARING)
    OUT  (HEAD_YAW),A
    LD   A,SKILL_SCRATCH
    OUT  (SKILL),A
    LD   A,GAIT_PAW_TEST
    OUT  (GAIT),A
    LD   A,EAR_FORWARD
    OUT  (EAR_POSE),A
    LD   A,TAIL_LEVEL
    OUT  (TAIL_POSE),A
    LD   A,VOCAL_SILENT
    OUT  (VOCAL),A
    LD   A,240
    OUT  (EYELID),A
    JP   survival_check
