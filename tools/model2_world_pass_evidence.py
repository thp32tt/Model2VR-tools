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


def _validate_report(report):
    if not isinstance(report, dict) or type(report.get("schema")) is not int or report["schema"] != 1 or (
        report.get("source_kind") != SOURCE_KIND or
        report.get("world_pass_proof") != "NOT_ESTABLISHED" or
        report.get("hmd_runtime_validation") != "UNTESTED"
    ):
        raise ValueError("expected an unproven real-source Model2 report (schema 1)")
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


def compare_sessions(race_reports, nonrace_reports):
    """Return conservative candidates, NEVER game-world authorization.

    At least two independently captured race sessions and a separate
    nonrace/title/menu session are required. Camera motion, units, world/HUD,
    emulator signature and HMD visuals still require private human proof.
    """
    if len(race_reports) < 2 or len(nonrace_reports) < 1:
        raise ValueError("need two race captures and at least one nonrace capture")
    race = [_validate_report(report) for report in race_reports]
    nonrace = [_validate_report(report) for report in nonrace_reports]
    all_callers = set().union(*(set(r) for r in race))
    nonrace_callers = set().union(*(set(r) for r in nonrace))
    candidates = []
    rejected = []
    for family, caller in sorted(all_callers):
        key = (family, caller)
        reason = None
        if int(caller, 16) == 0:
            reason = "unknown_gpu_caller"
        elif key in nonrace_callers:
            reason = "caller_also_used_outside_race"
        elif any(key not in session for session in race):
            reason = "missing_in_race_capture"
        elif any(not _enough_original_source(session[key]) for session in race):
            reason = "missing_same_draw_matrix_material_or_render_evidence"
        else:
            callers = [_cpu_callers(session[key]) for session in race]
            if any(c != callers[0] for c in callers[1:]):
                reason = "conflicting_original_cpu_projector_callers"
        if reason:
            rejected.append({"draw_family": family, "caller_rva_hex": caller,
                             "reason": reason})
        else:
            candidates.append({
                "draw_family": family,
                "caller_rva_hex": caller,
                "cpu_projector_caller_rva_hex": next(iter(callers[0])),
                "race_session_count": len(race),
                "per_race_source_present_counts": [
                    session[key]["distinct_projected_material_present_frame_count"]
                    for session in race
                ],
                "per_race_physically_presented_frame_counts": [
                    session[key]["distinct_physically_presented_material_present_frame_count"]
                    for session in race
                ],
                "per_race_original_draw_ordinal_ranges": [
                    [session[key]["original_draw_ordinal_min"],
                     session[key]["original_draw_ordinal_max"]]
                    for session in race
                ],
                "per_race_original_scene_ordinal_ranges": [
                    [session[key]["original_scene_ordinal_min"],
                     session[key]["original_scene_ordinal_max"]]
                    for session in race
                ],
                "evidence": "phase_attested_same_witness_source_candidate_only",
                "world_pass_authorized": False,
            })
    return {
        "schema": 1,
        "source_kind": "private_cross_phase_original_draw_candidate_triage",
        "race_session_count": len(race),
        "nonrace_session_count": len(nonrace),
        "world_pass_proof": "NOT_ESTABLISHED",
        "camera_axes_and_meters_per_unit": "NOT_CALIBRATED",
        "adapter_auto_enable": False,
        "hmd_runtime_validation": "UNTESTED",
        "candidates": candidates,
        "rejected": rejected,
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--race", action="append", required=True, type=Path,
                        help="private race CSV or model2_draw_source_report JSON; repeat")
    parser.add_argument("--nonrace", action="append", required=True, type=Path,
                        help="private menu/title/aux CSV or JSON; repeat")
    parser.add_argument("--output", type=Path, help="private local JSON destination")
    args = parser.parse_args()
    paths = args.race + args.nonrace
    if len(args.race) < 2 or len(set(p.resolve() for p in paths)) != len(paths):
        parser.error("two independent race files and distinct capture paths required")
    try:
        fingerprints = [_capture_digest(path) for path in paths]
        if len(set(fingerprints)) != len(fingerprints):
            parser.error("reused identical race/menu log bytes are not independent sessions")
        result = compare_sessions(
            [_load_report(path) for path in args.race],
            [_load_report(path) for path in args.nonrace],
        )
    except (ValueError, OSError, json.JSONDecodeError) as exc:
        parser.error(str(exc))
    content = json.dumps(result, sort_keys=True, indent=2) + "\n"
    if args.output:
        args.output.write_text(content, encoding="utf-8")
    else:
        print(content, end="")


if __name__ == "__main__":
    main()
