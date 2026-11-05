#!/usr/bin/env python3
"""Cross-session *candidate* triage for authentic original Daytona Draw evidence.

Run locally on PRIVATE race/menu captures. This never approves a world pass,
infers game camera calibration, emits source binaries, or enables stereo.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path

try:
    from .model2_draw_source_report import audit_csv
except ImportError:  # direct invocation: python tools/model2_world_pass_evidence.py
    from model2_draw_source_report import audit_csv

SOURCE_KIND = "real_d3d9_command_boundary_readonly_metadata"
RVA = re.compile(r"0x[0-9a-fA-F]{8}\Z")
MIN_PRESENTS = 2
MAX_SOURCES = 100_000
CONTROL_TYPES = frozenset(("title", "menu", "sky", "hud", "aux"))

def _mix_original_caller(rva):
    # Exact unsigned-32 arithmetic from the physical x86 Draw observer.
    value = (rva ^ (rva >> 16)) & 0xffffffff
    value = (value * 0x7feb352d) & 0xffffffff
    value ^= value >> 15
    value = (value * 0x846ca68b) & 0xffffffff
    return (value ^ (value >> 16)) & 0xffffffff

def original_caller_bloom_bits(caller):
    if not 0 < caller <= 0xffffffff:
        return 0, 0
    return (1 << (_mix_original_caller(caller) & 63),
            1 << (_mix_original_caller(caller ^ 0x9e3779b9) & 63))

def _control_bloom(report):
    record=report.get("original_all_family_caller_bloom")
    if not isinstance(record,dict) or set(record)!={
        "lane0","lane1","unknown_caller","first_present_frame",
        "last_present_frame","physical_present_count"}:
        raise ValueError("missing complete original all-family Draw caller control")
    lo,hi=record["lane0"],record["lane1"]
    count=record["physical_present_count"]
    last=record["last_present_frame"]
    if (type(lo) is not int or type(hi) is not int or
        not 0 <= lo <= 0xffffffffffffffff or
        not 0 <= hi <= 0xffffffffffffffff or
        type(record["unknown_caller"]) is not bool or
        record["unknown_caller"] or
        type(count) is not int or
        type(last) is not int or
        record["first_present_frame"] != 1 or
        not 1 <= count == last <= 0xffffffff or
        not (lo and hi)):
        raise ValueError("incomplete or unknown original all-family Draw caller control")
    return lo,hi

def _bloom_might_contain(bloom, caller):
    lo,hi=original_caller_bloom_bits(int(caller,16))
    return bool(lo and hi and bloom[0]&lo and bloom[1]&hi)


def _control_exact_callers(report,bloom):
    record=report.get("original_all_family_exact_caller_inventory")
    keys={"callers","unique_caller_count","overflow","unknown_caller",
          "first_present_frame","last_present_frame","physical_present_count"}
    if not isinstance(record,dict) or set(record)!=keys:
        raise ValueError("missing exact original four-family negative-control callers")
    raw=record["callers"]
    if (not isinstance(raw,list) or len(raw)>4096 or
        type(record["unique_caller_count"]) is not int or
        record["unique_caller_count"]!=len(raw) or
        type(record["overflow"]) is not bool or record["overflow"] or
        type(record["unknown_caller"]) is not bool or record["unknown_caller"] or
        type(record["first_present_frame"]) is not int or
        record["first_present_frame"]!=1 or
        type(record["last_present_frame"]) is not int or
        type(record["physical_present_count"]) is not int or
        not 1<=record["last_present_frame"]==
        record["physical_present_count"]<=0xffffffff):
        raise ValueError("incomplete exact original four-family negative-control callers")
    callers=set()
    expected_lo=expected_hi=0
    for value in raw:
        if (not isinstance(value,str) or RVA.fullmatch(value) is None or
            int(value,16)==0 or value.lower() in callers):
            raise ValueError("invalid exact original four-family caller entry")
        callers.add(value.lower())
        lo,hi=original_caller_bloom_bits(int(value,16))
        expected_lo|=lo
        expected_hi|=hi
    # Bloom is retained for compatibility/diagnostic provenance but is NOT
    # sufficient to exclude an exact game caller: after enough unique
    # game Draws, all bits may saturate and create spurious exclusions.
    if not callers or (expected_lo,expected_hi)!=bloom:
        raise ValueError("mismatched exact original four-family caller coverage")
    sampled=report.get("sources")
    if any(source["caller_rva_hex"].lower() not in callers
           for source in sampled):
        raise ValueError("sampled original Draw absent from exact caller coverage")
    return callers


def _validate_report(report):
    if not isinstance(report, dict) or type(report.get("schema")) is not int or report["schema"] != 1 or (
        report.get("source_kind") != SOURCE_KIND or
        report.get("world_pass_proof") != "NOT_ESTABLISHED" or
        report.get("hmd_runtime_validation") != "UNTESTED"
    ):
        raise ValueError("expected an unproven real-source Model2 report (schema 1)")
    run=report.get("original_capture_run")
    exact=report.get("original_all_family_exact_caller_inventory")
    bloom=report.get("original_all_family_caller_bloom")
    if (not isinstance(run,dict) or
        set(run)!={"nonce_hex","first_present_frame","last_present_frame",
                  "physical_present_count"} or
        not isinstance(run["nonce_hex"],str) or
        re.fullmatch(r"0x[0-9a-f]{16}",run["nonce_hex"]) is None or
        int(run["nonce_hex"],16)==0 or
        type(run["first_present_frame"]) is not int or
        run["first_present_frame"]!=1 or
        type(run["last_present_frame"]) is not int or
        not 1<=run["last_present_frame"]<=0xffffffff or
        type(run["physical_present_count"]) is not int or
        run["physical_present_count"]!=run["last_present_frame"]):
        raise ValueError("missing or inconsistent original capture run identity")
    if (isinstance(exact,dict) and
        exact.get("physical_present_count")!=run["physical_present_count"]):
        raise ValueError("inconsistent exact original capture run identity")
    if (isinstance(bloom,dict) and
        bloom.get("physical_present_count")!=run["physical_present_count"]):
        raise ValueError("inconsistent original all-family capture run identity")
    scene=report.get("original_physical_scene_draw_coverage")
    if (not isinstance(scene,dict) or
        set(scene)!={"physical_present_count","first_present_frame",
                    "last_present_frame","original_begin_scene_count",
                    "original_end_scene_count",
                    "original_successful_in_scene_draw_count",
                    "original_successful_untrusted_scene_draw_count",
                    "original_failed_draw_count","flagged_present_count",
                    "trusted_scene_drawn_present_count",
                    "all_original_scene_draws_trusted"} or
        any(type(scene.get(key)) is not int or
            scene[key]<0 or scene[key]>MAX_SOURCES*10000000
            for key in ("physical_present_count","first_present_frame",
                        "last_present_frame","original_begin_scene_count",
                        "original_end_scene_count",
                        "original_successful_in_scene_draw_count",
                        "original_successful_untrusted_scene_draw_count",
                        "original_failed_draw_count","flagged_present_count",
                        "trusted_scene_drawn_present_count")) or
        scene["first_present_frame"]!=1 or
        scene["physical_present_count"]!=run["physical_present_count"] or
        scene["last_present_frame"]!=run["physical_present_count"] or
        scene["original_begin_scene_count"]!=scene["original_end_scene_count"] or
        scene["original_successful_in_scene_draw_count"]<1 or
        scene["original_successful_untrusted_scene_draw_count"]!=0 or
        scene["original_failed_draw_count"]!=0 or
        scene["flagged_present_count"]!=0 or
        not 1<=scene["trusted_scene_drawn_present_count"]<=
            scene["physical_present_count"] or
        scene["all_original_scene_draws_trusted"] is not True):
        raise ValueError("incomplete physical original Scene/Draw ownership")
    # Every original x86 physical Present must have genuine RT0/depth
    # source Draw coverage in a NEW capture. A result from the earlier probe
    # without this continuous witness cannot assert native world ownership.
    target=report.get("original_physical_draw_target_coverage")
    target_keys={"physical_present_count","first_present_frame",
                 "last_present_frame","original_backbuffer_draw_count",
                 "original_auxiliary_draw_count","original_unknown_target_draw_count",
                 "original_failed_draw_count","original_incompatible_depth_draw_count",
                 "flagged_present_count","trusted_backbuffer_present_count",
                 "all_original_draw_targets_trusted"}
    target_numbers=target_keys-{"all_original_draw_targets_trusted"}
    if (not isinstance(target,dict) or set(target)!=target_keys or
        any(type(target.get(key)) is not int or
            not 0<=target[key]<=MAX_SOURCES*10000000
            for key in target_numbers) or
        type(target.get("all_original_draw_targets_trusted")) is not bool or
        target["first_present_frame"]!=1 or
        target["physical_present_count"]!=run["physical_present_count"] or
        target["last_present_frame"]!=run["physical_present_count"] or
        target["trusted_backbuffer_present_count"]>
            target["physical_present_count"] or
        target["flagged_present_count"]>target["physical_present_count"] or
        (target["original_backbuffer_draw_count"]+
         target["original_auxiliary_draw_count"]+
         target["original_unknown_target_draw_count"]+
         target["original_failed_draw_count"] !=
         scene["original_successful_in_scene_draw_count"]+
         scene["original_successful_untrusted_scene_draw_count"]+
         scene["original_failed_draw_count"] and
         target["flagged_present_count"]==0) or
        target["original_incompatible_depth_draw_count"]>
           target["original_backbuffer_draw_count"]+
           target["original_auxiliary_draw_count"]+
           target["original_unknown_target_draw_count"] or
        (target["all_original_draw_targets_trusted"] and (
            target["flagged_present_count"] or
            target["original_unknown_target_draw_count"] or
            target["original_failed_draw_count"] or
            target["original_incompatible_depth_draw_count"]))):
        raise ValueError("incomplete original physical RT0/depth Draw coverage")
    # Controls are allowed to be authentic 2D/no-depth captures: their
    # negative evidence is useful precisely because they are NOT World3D.
    # Race candidates, however, require a fully source-owned RT/depth path.
    # An empty title/menu/sky/HUD/aux report from a crashed or unhooked
    # emulator must never count as a negative scene witness. The native
    # original post-COM Present event is independent of VB/IB sampling and
    # includes real primitive counts for ALL four Draw families.
    sampled=report.get("original_control_present_sample_count")
    succeeded=report.get("original_control_successful_present_count")
    drawn=report.get("original_control_successful_drawn_present_count")
    families=report.get("original_control_four_family_draw_totals")
    expected={"DrawPrimitive","DrawIndexedPrimitive",
              "DrawPrimitiveUP","DrawIndexedPrimitiveUP"}
    if (any(type(value) is not int or value<1 or value>MAX_SOURCES
            for value in (sampled,succeeded,drawn)) or
        not drawn<=succeeded<=sampled or
        not isinstance(families,dict) or set(families)!=expected or
        any(type(v) is not int or v<0 or v>MAX_SOURCES*10000000
            for v in families.values()) or
        not any(families.values())):
        raise ValueError("missing authenticated original physical control Present")
    sources = report.get("sources")
    if not isinstance(sources, list) or len(sources) > MAX_SOURCES:
        raise ValueError("invalid bounded source list")
    result = {}
    for source in sources:
        if not isinstance(source, dict):
            raise ValueError("invalid source record")
        family = source.get("draw_family")
        caller = source.get("caller_rva_hex")
        if family not in ("DrawPrimitive", "DrawIndexedPrimitive") or (
            not isinstance(caller, str) or RVA.fullmatch(caller) is None
        ):
            raise ValueError("invalid real Draw family or caller RVA")
        key = (family, caller.lower())
        # Caller provenance must agree with the native executable RVA.
        if source.get("caller_is_in_executable") is not (int(caller, 16) != 0):
            raise ValueError("inconsistent original executable caller ownership")
        if key in result:
            raise ValueError("duplicate original Draw caller in a capture")
        scene_attested=source.get(
            "distinct_scene_attested_physically_presented_material_present_frame_count")
        if (scene_attested is not None and
            (type(scene_attested) is not int or scene_attested<0 or
             scene_attested>scene["trusted_scene_drawn_present_count"])):
            raise ValueError("per-caller Scene/Draw exceeds physical original frame coverage")
        result[key] = source
    return result


def _cpu_callers(source):
    records = source.get("same_witness_joint_cpu_matrix_commands", [])
    if not isinstance(records, list) or not records:
        return None
    callers = set()
    for record in records:
        # The tuple must have real CPU caller, matrix pointer, fingerprint
        # and geometry command; never treat a partial join as exact origin.
        if not isinstance(record, list) or len(record) != 4:
            return None
        if any(not isinstance(part, str) or RVA.fullmatch(part) is None
               or int(part, 16) == 0 for part in record):
            return None
        callers.add(record[0].lower())
    return callers if len(callers) == 1 else None


def _enough_original_source(source):
    # Projected-material is a SAME-witness subset of the complete CPU
    # projector + active matrix + geometry command + main RT/Present join.
    # Separate merely coincident counters must not be joined here.
    for field in (
        "distinct_projected_material_present_frame_count",
        "distinct_successful_projected_material_present_frame_count",
        "distinct_physically_presented_material_present_frame_count",
        "distinct_phase_attested_physically_presented_material_present_frame_count",
        "distinct_scene_attested_physically_presented_material_present_frame_count",
        "distinct_same_witness_joint_present_frame_count",
        "distinct_context_present_frame_count",
    ):
        value = source.get(field)
        if type(value) is not int or value < MIN_PRESENTS:
            return False
    for field in (
        "original_draw_failure_witnesses",
        "original_draw_result_missing_witnesses",
        "original_draw_result_inconsistent_witnesses",
        "original_draw_phase_missing_witnesses",
        "original_draw_phase_inconsistent_witnesses",
        "original_scene_phase_missing_witnesses",
        "original_scene_phase_inactive_witnesses",
        "original_scene_phase_inconsistent_witnesses",
        "original_scene_transition_failure_witnesses",
        "original_clear_failure_witnesses",
        "original_surface_failure_witnesses",
        "other_original_draw_failure_witnesses",
        "original_present_failed_witnesses",
        "original_present_missing_witnesses",
    ):
        if type(source.get(field)) is not int or source[field] != 0:
            return False
    # Phase order is the real all-family original Draw ordinal, not the
    # sampling ordinal. A JSON report without its new source evidence cannot
    # elevate an older static caller match into a gameplay race candidate.
    lo = source.get("original_draw_ordinal_min")
    hi = source.get("original_draw_ordinal_max")
    distinct = source.get("distinct_original_draw_ordinals")
    if (type(lo) is not int or type(hi) is not int or
            type(distinct) is not int or
            not 0 < lo <= hi <= 10_000_000 or
            not 0 < distinct <= hi - lo + 1):
        return False
    scene_lo = source.get("original_scene_ordinal_min")
    scene_hi = source.get("original_scene_ordinal_max")
    scene_distinct = source.get("distinct_original_scene_ordinals")
    if (type(scene_lo) is not int or type(scene_hi) is not int or
            type(scene_distinct) is not int or
            not 0 < scene_lo <= scene_hi <= 100_000 or
            not 0 < scene_distinct <= scene_hi - scene_lo + 1):
        return False
    for key, allowed in (
        ("original_source_vertex_pipeline_status", {"supported"}),
        ("original_source_fill_status", {"solid"}),
        ("original_source_depth_cull_status", {"supported"}),
    ):
        status = source.get(key)
        if (not isinstance(status, list) or not status or
                any(not isinstance(s, str) for s in status) or
                set(status) != allowed):
            return False
    return _cpu_callers(source) is not None


def _control_projector_callers(control):
    """Any readable CPU source caller in a non-race capture is negative evidence.

    HUD/sky/menu projections need not form a complete world candidate. An
    invalid non-race CPU record must never silently hide a caller collision.
    """
    callers=set()
    for source in control.values():
        records=source.get("same_witness_joint_cpu_matrix_commands",[])
        if not isinstance(records,list):
            raise ValueError("invalid nonrace CPU projector provenance")
        for record in records:
            if (not isinstance(record,list) or len(record)!=4 or
                any(not isinstance(item,str) or RVA.fullmatch(item) is None
                    for item in record) or int(record[0],16)==0):
                raise ValueError("invalid nonrace CPU projector provenance")
            callers.add(record[0].lower())
    return callers


def compare_sessions(race_reports, nonrace_reports):
    """Seven explicitly labelled private source captures: candidate-only.

    Two independent races and title/menu/sky/hud/aux NEGATIVE controls are
    required. Neither caller matching nor class labels prove actual world.
    """
    if not isinstance(race_reports,list) or len(race_reports)<2:
        raise ValueError("need two independent original race captures")
    if not isinstance(nonrace_reports,dict) or set(nonrace_reports)!=CONTROL_TYPES:
        raise ValueError("need exact title/menu/sky/hud/aux negative controls")
    race=[_validate_report(report) for report in race_reports]
    nonrace={label:_validate_report(nonrace_reports[label])
             for label in sorted(CONTROL_TYPES)}
    run_nonces=[r["original_capture_run"]["nonce_hex"] for r in race_reports]
    run_nonces += [nonrace_reports[label]["original_capture_run"]["nonce_hex"]
                   for label in sorted(CONTROL_TYPES)]
    if len(set(run_nonces)) != len(run_nonces):
        raise ValueError("seven independent original emulator processes required")
    all_callers=set().union(*(set(r) for r in race))
    # Original game can call one EXE RVA with different primitive families.
    control_gpu={label:{caller for _,caller in sources}
                 for label,sources in nonrace.items()}
    control_cpu={label:_control_projector_callers(sources)
                 for label,sources in nonrace.items()}
    # The sampled VB caller list misses original UP-only control phases.
    # Cross-check a cumulative unsampled four-family ORIGINAL x86 Draw
    # signature for EVERY distinct private control before any candidate.
    control_blooms={label:_control_bloom(nonrace_reports[label])
                    for label in sorted(CONTROL_TYPES)}
    control_exact={label:_control_exact_callers(
        nonrace_reports[label],control_blooms[label])
        for label in sorted(CONTROL_TYPES)}
    candidates=[]
    rejected=[]
    for family,caller in sorted(all_callers):
        key=(family,caller)
        reason=None
        if int(caller,16)==0:
            reason="unknown_gpu_caller"
        elif any(caller in rv_as for rv_as in control_gpu.values()):
            reason="caller_also_used_outside_race"
        elif any(caller in caller_set for caller_set in control_exact.values()):
            reason="caller_exactly_used_outside_race"
        elif any(key not in session for session in race):
            reason="missing_in_race_capture"
        elif any(not r["original_physical_draw_target_coverage"][
                "all_original_draw_targets_trusted"] for r in race_reports):
            reason="race_original_rt0_depth_draw_provenance_untrusted"
        elif any(not _enough_original_source(session[key]) for session in race):
            reason="missing_same_draw_matrix_material_or_render_evidence"
        else:
            callers=[_cpu_callers(session[key]) for session in race]
            if any(c!=callers[0] for c in callers[1:]):
                reason="conflicting_original_cpu_projector_callers"
            elif any(next(iter(callers[0])) in sources
                     for sources in control_cpu.values()):
                reason="cpu_projector_also_used_outside_race"
        if reason:
            rejected.append({"draw_family":family,"caller_rva_hex":caller,
                             "reason":reason})
        else:
            candidates.append({
                "draw_family":family,
                "caller_rva_hex":caller,
                "cpu_projector_caller_rva_hex":next(iter(callers[0])),
                "race_session_count":len(race),
                "per_race_source_present_counts":[
                    session[key]["distinct_projected_material_present_frame_count"]
                    for session in race],
                "per_race_physically_presented_frame_counts":[
                    session[key]["distinct_physically_presented_material_present_frame_count"]
                    for session in race],
                "per_race_original_draw_ordinal_ranges":[
                    [session[key]["original_draw_ordinal_min"],
                     session[key]["original_draw_ordinal_max"]]
                    for session in race],
                "per_race_original_scene_ordinal_ranges":[
                    [session[key]["original_scene_ordinal_min"],
                     session[key]["original_scene_ordinal_max"]]
                    for session in race],
                "evidence":"phase_attested_same_witness_source_candidate_only",
                "world_pass_authorized":False,
            })
    return {
        "schema":1,
        "source_kind":"private_cross_phase_original_draw_candidate_triage",
        "race_session_count":len(race),
        "nonrace_session_count":len(nonrace),
        "control_types":sorted(CONTROL_TYPES),
        "complete_negative_controls":True,
        "world_pass_proof":"NOT_ESTABLISHED",
        "camera_axes_and_meters_per_unit":"NOT_CALIBRATED",
        "adapter_auto_enable":False,
        "hmd_runtime_validation":"UNTESTED",
        "candidates":candidates,
        "rejected":rejected,
    }


def _capture_digest(path):
    # Duplicate bytes under distinct filenames are NOT independent sessions.
    # Stream large private CSV logs without retaining them in memory.
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.digest()


def _load_report(path):
    if path.suffix.lower() == ".csv":
        return audit_csv(path)
    with path.open(encoding="utf-8") as handle:
        return json.load(handle)


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--race",action="append",required=True,type=Path,
                        help="private original race CSV/report; provide at least two")
    parser.add_argument("--control",action="append",nargs=2,required=True,
                        metavar=("CLASS","PRIVATE_CSV_OR_JSON"),
                        help="exactly one for each title/menu/sky/hud/aux")
    parser.add_argument("--output",type=Path,
                        help="PRIVATE local-only JSON candidate report")
    args=parser.parse_args(argv)
    controls={}
    for label,raw in args.control:
        if label not in CONTROL_TYPES or label in controls:
            parser.error("unique --control classes must be title/menu/sky/hud/aux")
        controls[label]=Path(raw)
    if set(controls)!=CONTROL_TYPES:
        parser.error("all five title/menu/sky/hud/aux controls required")
    paths=args.race+list(controls.values())
    if len(args.race)<2 or len(set(p.resolve() for p in paths))!=len(paths):
        parser.error("two independent race files and distinct capture paths required")
    try:
        fingerprints=[_capture_digest(path) for path in paths]
        if len(set(fingerprints))!=len(fingerprints):
            parser.error("identical capture bytes cannot prove independent sessions")
        result=compare_sessions(
            [_load_report(path) for path in args.race],
            {label:_load_report(path) for label,path in controls.items()},
        )
    except (ValueError,OSError,json.JSONDecodeError) as exc:
        parser.error(str(exc))
    content=json.dumps(result,sort_keys=True,indent=2)+"\n"
    if args.output:
        args.output.write_text(content,encoding="utf-8")
    else:
        print(content,end="")


if __name__=="__main__":
    main()
