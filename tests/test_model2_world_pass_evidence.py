import copy
import itertools
import unittest
from tools.model2_world_pass_evidence import (
    compare_sessions, CONTROL_TYPES, main, original_caller_bloom_bits,
    preflight_original_session)
from contextlib import redirect_stderr, redirect_stdout
from unittest.mock import patch
from pathlib import Path
import io
import json
import tempfile


def witness(cpu="0x004bb400", count=3):
    return {
        "draw_family": "DrawIndexedPrimitive",
        "caller_rva_hex": "0x004a1234",
        "caller_is_in_executable": True,
        "distinct_projected_material_present_frame_count": count,
        "distinct_successful_projected_material_present_frame_count": count,
        "original_draw_success_witnesses": count,
        "original_draw_failure_witnesses": 0,
        "original_draw_result_missing_witnesses": 0,
        "original_draw_result_inconsistent_witnesses": 0,
        "distinct_physically_presented_material_present_frame_count": count,
        "distinct_phase_attested_physically_presented_material_present_frame_count": count,
        "distinct_scene_attested_physically_presented_material_present_frame_count": count,
        "distinct_post_com_retained_present_frame_count": count,
        "original_draw_completion_missing_witnesses": 0,
        "original_draw_completion_untrusted_witnesses": 0,
        "original_draw_completion_inconsistent_witnesses": 0,
        "original_scene_phase_missing_witnesses": 0,
        "original_scene_phase_inactive_witnesses": 0,
        "original_scene_phase_inconsistent_witnesses": 0,
        "original_scene_transition_failure_witnesses": 0,
        "original_clear_failure_witnesses": 0,
        "original_surface_failure_witnesses": 0,
        "other_original_draw_failure_witnesses": 0,
        "original_scene_ordinal_min": 1,
        "original_scene_ordinal_max": 2,
        "distinct_original_scene_ordinals": 2,
        "original_draw_phase_missing_witnesses": 0,
        "original_draw_phase_inconsistent_witnesses": 0,
        "original_draw_ordinal_min": 12,
        "original_draw_ordinal_max": 20,
        "distinct_original_draw_ordinals": 3,
        "original_present_failed_witnesses": 0,
        "original_present_missing_witnesses": 0,
        "distinct_same_witness_joint_present_frame_count": count,
        "distinct_cpu_fit_population_present_frame_count": count,
        "cpu_fit_population_missing_batches": 0,
        "distinct_context_present_frame_count": count,
        "original_source_vertex_pipeline_status": ["supported"],
        "original_source_fill_status": ["solid"],
        "original_source_depth_cull_status": ["supported"],
        "same_witness_joint_cpu_matrix_commands": [
            [cpu, "0x010f0068", "0x12345678", "0x00000120"],
            [cpu, "0x010f00a8", "0x87654321", "0x00000121"],
        ],
    }


_run_nonce_counter=itertools.count(1)

def report(*sources):
    rvas={0x004f5000}
    for source in sources:
        caller=source.get("caller_rva_hex") if isinstance(source,dict) else None
        if isinstance(caller,str) and caller.startswith("0x"):
            try:
                decoded=int(caller,16)
            except ValueError:
                continue
            if 0<decoded<=0xffffffff:
                rvas.add(decoded)
    bits=[original_caller_bloom_bits(value) for value in rvas]
    lo,hi=(0,0)
    for a,b in bits:
        lo|=a
        hi|=b
    return {
        "schema": 1,
        "source_kind": "real_d3d9_command_boundary_readonly_metadata",
        "world_pass_proof": "NOT_ESTABLISHED",
        "hmd_runtime_validation": "UNTESTED",
        "original_physical_scene_draw_coverage":{
            "physical_present_count":8,"first_present_frame":1,
            "last_present_frame":8,
            "original_begin_scene_count":3,"original_end_scene_count":3,
            "original_successful_in_scene_draw_count":9,
            "original_successful_untrusted_scene_draw_count":0,
            "original_failed_draw_count":0,"flagged_present_count":0,
            "trusted_scene_drawn_present_count":3,
            "all_original_scene_draws_trusted":True,
        },
        "original_physical_camera_com_lifetime":{
            "physical_present_count":8,"first_present_frame":1,
            "last_present_frame":8,
            "original_com_draw_count":9,
            "original_camera_write_count":15,
            "original_camera_state_write_count":5,
            "original_pixel_program_write_count":7,
            "original_vertex_layout_write_count":3,
            "flagged_present_count":0,
            "trusted_camera_drawn_present_count":3,
            "all_original_camera_com_lifetimes_trusted":True,
        },
        "original_physical_draw_target_coverage":{
            "physical_present_count":8,"first_present_frame":1,
            "last_present_frame":8,
            "original_backbuffer_draw_count":9,
            "original_auxiliary_draw_count":0,
            "original_unknown_target_draw_count":0,
            "original_failed_draw_count":0,
            "original_incompatible_depth_draw_count":0,
            "flagged_present_count":0,
            "trusted_backbuffer_present_count":3,
            "all_original_draw_targets_trusted":True,
        },
        "original_capture_run":{
            "nonce_hex":f"0x{next(_run_nonce_counter):016x}",
            "first_present_frame":1,"last_present_frame":8,
            "physical_present_count":8,
        },
        "original_all_family_exact_caller_inventory":{
            "callers":[f"0x{value:08x}" for value in sorted(rvas)],
            "unique_caller_count":len(rvas),
            "overflow":False,"unknown_caller":False,
            "first_present_frame":1,
            "last_present_frame":8,
            "physical_present_count":8,
        },
        "original_all_family_caller_bloom":{
            "lane0":lo,
            "lane1":hi,
            "unknown_caller":False,
            "first_present_frame":1,
            "last_present_frame":8,
            "physical_present_count":8,
        },
        "original_control_present_sample_count":3,
        "original_control_successful_present_count":3,
        "original_control_successful_drawn_present_count":3,
        "original_control_four_family_draw_totals":{
            "DrawPrimitive":0,"DrawIndexedPrimitive":9,
            "DrawPrimitiveUP":0,"DrawIndexedPrimitiveUP":0,
        },
        "sources": list(sources),
    }


def controls(**overrides):
    return {label:overrides.get(label,report()) for label in CONTROL_TYPES}


class OriginalWorldPassEvidenceTest(unittest.TestCase):
    def test_single_original_process_preflight_before_seven_stage_capture(self):
        # Standalone preflight uses the exact same original Scene, camera
        # lifetime, target/depth, physical Present and caller inventory
        # verifier as seven-process comparison; never authorizes World3D.
        valid=report(witness())
        with tempfile.TemporaryDirectory() as dirname:
            csv=Path(dirname)/"private-original.csv"
            csv.write_text("unreadable test bytes",encoding="utf-8")
            cached=Path(dirname)/"private-original-report.json"
            with patch("tools.model2_world_pass_evidence.audit_csv",
                       return_value=valid) as observed:
                main(["--preflight",str(csv),"--output",str(cached)])
                self.assertEqual(observed.call_count,1)
                self.assertEqual(json.loads(cached.read_text("utf-8")),valid)
                with io.StringIO() as output,redirect_stdout(output):
                    main(["--preflight",str(csv)])
                    captured=output.getvalue()
                    summary=json.loads(captured)
                self.assertEqual(summary["preflight"],"ORIGINAL_SOURCE_READABLE")
                self.assertEqual(summary["physical_present_count"],8)
                self.assertEqual(summary["world_pass_proof"],"NOT_ESTABLISHED")
                self.assertEqual(summary["hmd_runtime_validation"],"UNTESTED")
                self.assertNotIn("caller_rva_hex",captured)
            with redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit):
                    main(["--preflight",str(csv),"--race",str(csv)])
            # A legacy/missing unsampled physical original run cannot
            # slip into the negative controls simply because it has
            # sampled GPU source records.
            bad=report(witness())
            bad["original_capture_run"]=None
            with patch("tools.model2_world_pass_evidence.audit_csv",
                       return_value=bad):
                with redirect_stderr(io.StringIO()):
                    with self.assertRaises(SystemExit):
                        main(["--preflight",str(csv),"--output",str(cached)])
            self.assertEqual(json.loads(cached.read_text("utf-8")),valid)
            with self.assertRaisesRegex(ValueError,"original x86 CSV"):
                preflight_original_session(cached)

    def test_sampled_race_scene_cannot_borrow_unrelated_physical_frame(self):
        race=report(witness())
        race["original_physical_scene_draw_coverage"][
            "trusted_scene_drawn_present_count"]=2
        with self.assertRaisesRegex(ValueError,"per-caller Scene/Draw"):
            compare_sessions([race,report(witness())],controls())
        race=report(witness())
        race["sources"][0][
            "distinct_scene_attested_physically_presented_material_present_frame_count"]=4
        with self.assertRaisesRegex(ValueError,"per-caller Scene/Draw"):
            compare_sessions([race,report(witness())],controls())

    def test_race_cpu_gpu_same_original_draw_requires_post_com_completion(self):
        # A pre-COM projected match cannot borrow another physical Draw's
        # successful scene or RT state after its actual COM return.
        race_a=report(witness())
        race_b=report(witness())
        valid=compare_sessions([race_a,race_b],controls())
        self.assertTrue(valid["candidates"])
        for field,bad in (
            ("distinct_post_com_retained_present_frame_count",0),
            ("original_draw_completion_missing_witnesses",1),
            ("original_draw_completion_untrusted_witnesses",1),
            ("original_draw_completion_inconsistent_witnesses",1),
        ):
            with self.subTest(field=field):
                race=report(witness())
                race["sources"][0][field]=bad
                result=compare_sessions([race,report(witness())],controls())
                self.assertEqual(result["candidates"],[])
                self.assertEqual(result["rejected"][0]["reason"],
                    "missing_same_draw_matrix_material_or_render_evidence")

    def test_original_camera_COM_lifetime_required_for_seven_sessions(self):
        # No sampled raster pass may borrow a clean camera from a different
        # original physical process, draw count, or completed COM epoch.
        for field,bad in (
            ("original_physical_camera_com_lifetime",None),
            ("physical_present_count",7),
            ("original_com_draw_count",10),
            ("original_camera_state_write_count",6),
            ("original_pixel_program_write_count",8),
            ("flagged_present_count",9),
            ("all_original_camera_com_lifetimes_trusted",1),
        ):
            with self.subTest(field=field):
                race=report(witness())
                if field=="original_physical_camera_com_lifetime":
                    del race[field]
                else:
                    race["original_physical_camera_com_lifetime"][field]=bad
                with self.assertRaisesRegex(
                    ValueError,"physical camera COM lifetime"):
                    compare_sessions([race,report(witness())],controls())
        race=report(witness())
        race["original_physical_camera_com_lifetime"]["flagged_present_count"]=1
        race["original_physical_camera_com_lifetime"][
            "all_original_camera_com_lifetimes_trusted"]=False
        decision=compare_sessions([race,report(witness())],controls())
        self.assertEqual(decision["candidates"],[])
        self.assertEqual(decision["rejected"][0]["reason"],
                         "race_original_camera_com_lifetime_untrusted")
        # A report can contain genuine pixel shader and vertex-layout
        # changes while ZERO original view/projection source calls occurred.
        race=report(witness())
        camera=race["original_physical_camera_com_lifetime"]
        camera["original_camera_state_write_count"]=0
        camera["original_pixel_program_write_count"]=12
        decision=compare_sessions([race,report(witness())],controls())
        self.assertEqual(decision["candidates"],[])
        self.assertEqual(decision["rejected"][0]["reason"],
                         "race_original_camera_state_write_missing")

    def test_original_rt0_depth_all_family_source_required_for_seven_sessions(self):
        # New original producer always emits physical RT0/depth evidence,
        # including unsampled UP families. Missing/altered reports cannot
        # inherit source RT data from an old successful Scene record.
        for field,bad in (
            ("original_physical_draw_target_coverage",None),
            ("physical_present_count",7),
            ("original_backbuffer_draw_count",10),
            ("original_unknown_target_draw_count",10),
            ("trusted_backbuffer_present_count",9),
            ("all_original_draw_targets_trusted",1),
        ):
            with self.subTest(field=field):
                race=report(witness())
                if field=="original_physical_draw_target_coverage":
                    del race[field]
                else:
                    race["original_physical_draw_target_coverage"][field]=bad
                with self.assertRaisesRegex(
                    ValueError,"physical RT0/depth Draw coverage"):
                    compare_sessions([race,report(witness())],controls())
        race_a=report(witness())
        # A positive-looking report cannot launder auxiliary RT0 draws.
        aux_race=report(witness())
        aux_target=aux_race["original_physical_draw_target_coverage"]
        aux_target["original_backbuffer_draw_count"]-=1
        aux_target["original_auxiliary_draw_count"]=1
        with self.assertRaisesRegex(
                ValueError,"physical RT0/depth Draw coverage"):
            compare_sessions([aux_race,report(witness())],controls())
        aux_target["all_original_draw_targets_trusted"]=False
        aux_target["flagged_present_count"]=1
        aux_candidates=compare_sessions([aux_race,report(witness())],controls())
        self.assertEqual(aux_candidates["candidates"],[])
        race_b=report(witness())
        race_b["original_physical_draw_target_coverage"][
            "all_original_draw_targets_trusted"]=False
        race_b["original_physical_draw_target_coverage"][
            "flagged_present_count"]=1
        result=compare_sessions([race_a,race_b],controls())
        self.assertEqual(result["candidates"],[])
        self.assertTrue(any(v["reason"]==
            "race_original_rt0_depth_draw_provenance_untrusted"
            for v in result["rejected"]))
        # Authentic 2D controls are not rejected just because they have
        # no compatible depth or source-owned World3D RT.
        menu=report()
        menu["original_physical_draw_target_coverage"][
            "flagged_present_count"]=1
        menu["original_physical_draw_target_coverage"][
            "all_original_draw_targets_trusted"]=False
        completed=compare_sessions(
            [report(witness()),report(witness())],controls(menu=menu))
        self.assertTrue(completed["complete_negative_controls"])
        self.assertFalse(completed["adapter_auto_enable"])

    def test_original_unsampled_scene_draw_controls_required_for_all_seven(self):
        for field,bad in (
            ("original_physical_scene_draw_coverage",None),
            ("original_successful_in_scene_draw_count",0),
            ("original_successful_untrusted_scene_draw_count",1),
            ("original_failed_draw_count",1),
            ("flagged_present_count",1),
            ("original_end_scene_count",2),
            ("all_original_scene_draws_trusted",False),
            ("physical_present_count",7),
        ):
            with self.subTest(field=field):
                c=report()
                if field=="original_physical_scene_draw_coverage":
                    del c[field]
                else:
                    c["original_physical_scene_draw_coverage"][field]=bad
                with self.assertRaisesRegex(ValueError,"physical original Scene/Draw"):
                    compare_sessions([report(witness()),report(witness())],
                                     controls(menu=c))
        good=compare_sessions([report(witness()),report(witness())],controls())
        self.assertTrue(good["complete_negative_controls"])
        self.assertFalse(good["adapter_auto_enable"])

    def test_same_native_process_reused_under_different_capture_bytes_rejected(self):
        race_a=report(witness())
        race_b=report(witness())
        race_b["original_capture_run"]=copy.deepcopy(
            race_a["original_capture_run"])
        # Even distinct source bytes, labels and paths cannot turn one real
        # x86 emulator process into two independent race observations.
        with self.assertRaisesRegex(ValueError,"independent original emulator"):
            compare_sessions([race_a,race_b],controls())
        race_a=report(witness())
        control=report()
        control["original_capture_run"]=copy.deepcopy(
            race_a["original_capture_run"])
        with self.assertRaisesRegex(ValueError,"independent original emulator"):
            compare_sessions([race_a,report(witness())],
                             controls(aux=control))

    def test_missing_malformed_or_frame_mismatched_run_witness_is_not_proof(self):
        for damaged in (
            None,
            {"nonce_hex":"0x0000000000000000","first_present_frame":1,
             "last_present_frame":8,"physical_present_count":8},
            {"nonce_hex":"0x0000000000000001","first_present_frame":2,
             "last_present_frame":8,"physical_present_count":8},
            {"nonce_hex":"0x0000000000000001","first_present_frame":1,
             "last_present_frame":7,"physical_present_count":7},
        ):
            with self.subTest(damaged=damaged):
                race=report(witness())
                race["original_capture_run"]=damaged
                with self.assertRaisesRegex(ValueError,"capture run identity"):
                    compare_sessions([race,report(witness())],controls())

    def test_clear_failure_field_cannot_be_omitted_or_replayed(self):
        clean=witness()
        del clean["original_clear_failure_witnesses"]
        got=compare_sessions([report(clean),report(witness())],controls())
        self.assertFalse(got["candidates"])
        self.assertEqual(got["rejected"][0]["reason"],
                         "missing_same_draw_matrix_material_or_render_evidence")
        bad=witness()
        bad["original_clear_failure_witnesses"]=1
        got=compare_sessions([report(bad),report(witness())],controls())
        self.assertFalse(got["candidates"])
        self.assertEqual(got["rejected"][0]["reason"],
                         "missing_same_draw_matrix_material_or_render_evidence")

    def test_cross_phase_conservative_candidate_only(self):
        a, b = witness(), witness()
        # Same CPU caller, different matrix/command fingerprints per
        # independent run; matrices can change while a car/camera moves.
        b["same_witness_joint_cpu_matrix_commands"][0][2] = "0xaaaaaaaa"
        got = compare_sessions([report(a), report(b)], controls())
        self.assertEqual(len(got["candidates"]), 1)
        self.assertFalse(got["adapter_auto_enable"])
        self.assertFalse(got["candidates"][0]["world_pass_authorized"])
        self.assertEqual(got["candidates"][0]
                         ["per_race_original_draw_ordinal_ranges"],
                         [[12, 20], [12, 20]])
        self.assertEqual(got["candidates"][0]
                         ["per_race_original_scene_ordinal_ranges"],
                         [[1, 2], [1, 2]])
        self.assertEqual(got["world_pass_proof"], "NOT_ESTABLISHED")
        self.assertEqual(got["hmd_runtime_validation"], "UNTESTED")

    def test_menu_reuse_blocks_promotion_even_if_joined_only_in_race(self):
        candidate = witness()
        neg = {"draw_family": candidate["draw_family"],
               "caller_rva_hex": candidate["caller_rva_hex"],
               "caller_is_in_executable": True}
        result = compare_sessions([report(candidate), report(candidate)],
                                  controls(menu=report(neg)))
        self.assertFalse(result["candidates"])
        self.assertEqual(result["rejected"][0]["reason"],
                         "caller_also_used_outside_race")

    def test_missing_or_unsupported_same_witness_metadata_fails_closed(self):
        for damaged in (
            {"distinct_projected_material_present_frame_count": 1},
            {"distinct_same_witness_joint_present_frame_count": 0},
            {"distinct_cpu_fit_population_present_frame_count": 0},
            {"distinct_cpu_fit_population_present_frame_count": 1},
            {"cpu_fit_population_missing_batches": 1},
            {"distinct_context_present_frame_count": 0},
            {"original_source_fill_status": ["solid", "unsupported"]},
            {"original_source_depth_cull_status": []},
            {"same_witness_joint_cpu_matrix_commands": []},
            {"same_witness_joint_cpu_matrix_commands": [
                ["0x004bb400", "0x00000000", "0x12345678", "0x00000120"]]},
        ):
            with self.subTest(damaged=damaged):
                first = witness()
                first.update(damaged)
                got = compare_sessions([report(first), report(witness())],
                                       controls())
                self.assertEqual(got["candidates"], [])
                self.assertEqual(len(got["rejected"]), 1)

    def test_original_hresult_failure_missing_or_wrong_frame_blocks_candidates(self):
        for damaged in (
            {"distinct_successful_projected_material_present_frame_count": 0},
            {"original_draw_failure_witnesses": 1},
            {"original_clear_failure_witnesses": 1},
            {"original_surface_failure_witnesses": 1},
            {"original_draw_result_missing_witnesses": 1},
            {"original_draw_result_inconsistent_witnesses": 1},
            {"distinct_physically_presented_material_present_frame_count": 0},
            {"original_present_failed_witnesses": 1},
            {"original_present_missing_witnesses": 1},
            {"distinct_phase_attested_physically_presented_material_present_frame_count": 0},
            {"original_draw_phase_missing_witnesses": 1},
            {"original_draw_phase_inconsistent_witnesses": 1},
            {"original_draw_ordinal_min": None},
            {"original_draw_ordinal_max": 10},
            {"original_draw_ordinal_max": 10, "original_draw_ordinal_min": 20},
            {"distinct_original_draw_ordinals": 0},
            {"distinct_scene_attested_physically_presented_material_present_frame_count": 0},
            {"original_scene_phase_missing_witnesses": 1},
            {"original_scene_phase_inactive_witnesses": 1},
            {"original_scene_phase_inconsistent_witnesses": 1},
            {"original_scene_transition_failure_witnesses": 1},
            {"other_original_draw_failure_witnesses": 1},
            {"original_scene_ordinal_min": None},
            {"original_scene_ordinal_min": 3, "original_scene_ordinal_max": 2},
            {"distinct_original_scene_ordinals": 0},
        ):
            with self.subTest(damaged=damaged):
                source = witness()
                source.update(damaged)
                result = compare_sessions([report(source), report(witness())],
                                          controls())
                self.assertEqual(result["candidates"], [])
                self.assertEqual(result["rejected"][0]["reason"],
                                 "missing_same_draw_matrix_material_or_render_evidence")

    def test_cpu_caller_drift_and_mixed_projector_fail_closed(self):
        got = compare_sessions(
            [report(witness()), report(witness("0x004bb500"))],
            controls())
        self.assertEqual(got["rejected"][0]["reason"],
                         "conflicting_original_cpu_projector_callers")
        mixed = witness()
        mixed["same_witness_joint_cpu_matrix_commands"][1][0] = "0x004bb500"
        got = compare_sessions([report(witness()), report(mixed)], controls())
        self.assertFalse(got["candidates"])

    def test_missing_race_and_unknown_callers(self):
        got = compare_sessions([report(witness()), report()], controls())
        self.assertEqual(got["rejected"][0]["reason"],
                         "missing_in_race_capture")
        unknown = witness()
        unknown["caller_rva_hex"] = "0x00000000"
        unknown["caller_is_in_executable"] = False
        got = compare_sessions([report(unknown), report(unknown)], controls())
        self.assertEqual(got["rejected"][0]["reason"], "unknown_gpu_caller")

    def test_spoofed_original_caller_and_malformed_states_rejected(self):
        bad = witness()
        bad["caller_is_in_executable"] = False
        with self.assertRaises(ValueError):
            compare_sessions([report(bad), report(witness())], controls())
        bad = witness()
        bad["original_source_fill_status"] = [["solid"]]
        got = compare_sessions([report(bad), report(witness())], controls())
        self.assertFalse(got["candidates"])

    def test_bad_source_kind_or_claimed_proof_rejected(self):
        good = report(witness())
        for field, value in (
            ("world_pass_proof", "ESTABLISHED"),
            ("source_kind", "synthetic"),
            ("hmd_runtime_validation", "PASS"),
            ("schema", 2),
        ):
            bad = copy.deepcopy(good)
            bad[field] = value
            with self.subTest(field=field):
                with self.assertRaises(ValueError):
                    compare_sessions([bad, good], controls())

    def test_duplicates_and_insufficient_phases_rejected(self):
        with self.assertRaises(ValueError):
            compare_sessions([report(witness())], controls())
        with self.assertRaises(ValueError):
            compare_sessions([report(witness(), witness()), report(witness())],
                             controls())


    def test_up_only_negative_control_catches_reused_race_caller(self):
        # No sampled VB/IB source in HUD: only real unsampled UP Draws
        # contributed to the monotonic native all-family caller signature.
        hud=report()
        caller=0x004a1234
        lo,hi=original_caller_bloom_bits(caller)
        existing=0x004f5000
        additional_lo,additional_hi=original_caller_bloom_bits(existing)
        hud["original_all_family_caller_bloom"].update(
            lane0=lo|additional_lo,lane1=hi|additional_hi)
        hud["original_all_family_exact_caller_inventory"].update(
            callers=[f"0x{value:08x}" for value in sorted((existing,caller))],
            unique_caller_count=2)
        hud["original_control_four_family_draw_totals"]={
            "DrawPrimitive":0,"DrawIndexedPrimitive":0,
            "DrawPrimitiveUP":5,"DrawIndexedPrimitiveUP":4}
        got=compare_sessions([report(witness()),report(witness())],
                             controls(hud=hud))
        self.assertEqual(got["candidates"],[])
        self.assertEqual(got["rejected"][0]["reason"],
                         "caller_exactly_used_outside_race")

    def test_missing_or_unknown_bloom_does_not_claim_complete_controls(self):
        for mutation in (
            None, {"unknown_caller":True},
            {"lane0":0}, {"physical_present_count":7},
        ):
            control=report()
            if mutation is None:
                del control["original_all_family_caller_bloom"]
            else:
                control["original_all_family_caller_bloom"].update(mutation)
            with self.assertRaisesRegex(ValueError,"all-family"):
                compare_sessions([report(witness()),report(witness())],
                                 controls(aux=control))

    def test_saturated_bloom_cannot_falsely_exclude_exact_race_caller(self):
        control=report()
        # Deliberate full bloom (indistinguishable from race in bitmap),
        # without an original exact callsite: reject the report as corrupted
        # instead of manufacturing positive/negative gameplay proof.
        control["original_all_family_caller_bloom"].update(
            lane0=0xffffffffffffffff,lane1=0xffffffffffffffff)
        with self.assertRaisesRegex(ValueError,"mismatched exact"):
            compare_sessions([report(witness()),report(witness())],
                             controls(aux=control))

    def test_fully_saturated_real_bloom_no_longer_excludes_unrelated_race(self):
        # A real long original title/menu session may touch >1000 unique
        # physical Draw callsites. Its two bloom lanes can legitimately
        # saturate, without sharing the specific candidate world caller.
        from tools.model2_world_pass_evidence import original_caller_bloom_bits
        callers=[0x00510000+i for i in range(1500)]
        lo=hi=0
        for rva in callers:
            a,b=original_caller_bloom_bits(rva)
            lo|=a
            hi|=b
        self.assertEqual(lo,0xffffffffffffffff)
        self.assertEqual(hi,0xffffffffffffffff)
        control=report()
        control["original_all_family_exact_caller_inventory"].update(
            callers=[f"0x{rva:08x}" for rva in callers],
            unique_caller_count=len(callers))
        control["original_all_family_caller_bloom"].update(lane0=lo,lane1=hi)
        got=compare_sessions([report(witness()),report(witness())],
                             controls(aux=control))
        self.assertEqual(len(got["candidates"]),1)
        self.assertFalse(got["adapter_auto_enable"])

    def test_missing_exact_caller_control_fails_closed(self):
        control=report()
        del control["original_all_family_exact_caller_inventory"]
        with self.assertRaisesRegex(ValueError,"exact original"):
            compare_sessions([report(witness()),report(witness())],
                             controls(menu=control))

    def test_original_gpu_caller_reused_with_different_primitive_family(self):
        original=witness()
        sky=witness()
        sky["draw_family"]="DrawPrimitive"
        got=compare_sessions([report(original),report(original)],
                             controls(sky=report(sky)))
        self.assertFalse(got["candidates"])
        self.assertEqual(got["rejected"][0]["reason"],
                         "caller_also_used_outside_race")

    def test_cpu_projector_alias_rejects_distinct_gpu_callers(self):
        original=witness()
        hud=witness()
        hud["caller_rva_hex"]="0x004f1234"
        got=compare_sessions([report(original),report(original)],
                             controls(hud=report(hud)))
        self.assertFalse(got["candidates"])
        self.assertEqual(got["rejected"][0]["reason"],
                         "cpu_projector_also_used_outside_race")

    def test_malformed_control_cpu_projection_cannot_hide_negative_evidence(self):
        for records in (
            ["bogus"],
            [["0x004bb400"]],
            [["0x00000000","0x010f0068","0x12345678","0x00000120"]],
        ):
            with self.subTest(records=records):
                bad=witness()
                bad["caller_rva_hex"]="0x004f1234"
                bad["same_witness_joint_cpu_matrix_commands"]=records
                with self.assertRaisesRegex(ValueError,
                                            "nonrace CPU projector provenance"):
                    compare_sessions([report(witness()),report(witness())],
                                     controls(aux=report(bad)))

    def test_physical_present_absent_or_unrendered_control_is_not_negative(self):
        for key,value in (
            ("original_control_present_sample_count",0),
            ("original_control_successful_present_count",0),
            ("original_control_successful_drawn_present_count",0),
            ("original_control_four_family_draw_totals",{}),
            ("original_control_four_family_draw_totals",{
                "DrawPrimitive":0,"DrawIndexedPrimitive":0,
                "DrawPrimitiveUP":0,"DrawIndexedPrimitiveUP":0}),
        ):
            with self.subTest(key=key,value=value):
                control=report()
                control[key]=value
                with self.assertRaisesRegex(ValueError,
                                            "original physical control Present"):
                    compare_sessions([report(witness()),report(witness())],
                                     controls(menu=control))
        missing=report()
        del missing["original_control_present_sample_count"]
        with self.assertRaisesRegex(ValueError,"original physical control Present"):
            compare_sessions([report(witness()),report(witness())],
                             controls(aux=missing))

    def test_missing_negative_control_label_is_rejected(self):
        for label in CONTROL_TYPES:
            observed=controls()
            del observed[label]
            with self.subTest(label=label):
                with self.assertRaisesRegex(ValueError,"title/menu/sky/hud/aux"):
                    compare_sessions([report(witness()),report(witness())],
                                     observed)
        good=compare_sessions([report(witness()),report(witness())],controls())
        self.assertTrue(good["complete_negative_controls"])
        self.assertEqual(good["control_types"],sorted(CONTROL_TYPES))
        self.assertEqual(len(good["candidates"]),1)
        self.assertFalse(good["adapter_auto_enable"])
        self.assertFalse(good["candidates"][0]["world_pass_authorized"])

    def test_cli_requires_distinct_bytes_and_all_five_named_classes(self):
        with tempfile.TemporaryDirectory() as dirname:
            root=Path(dirname)
            captures={}
            for label in ("race_a","race_b",*sorted(CONTROL_TYPES)):
                source=report(witness()) if label.startswith("race") else report()
                source["test_session_identity"]=label
                destination=root/(label+".json")
                destination.write_text(json.dumps(source),encoding="utf-8")
                captures[label]=destination
            argv=["--race",str(captures["race_a"]),
                  "--race",str(captures["race_b"])]
            for label in sorted(CONTROL_TYPES):
                argv.extend(["--control",label,str(captures[label])])
            output=root/"private-local-triage.json"
            main(argv+["--output",str(output)])
            data=json.loads(output.read_text("utf-8"))
            self.assertEqual(len(data["candidates"]),1)
            self.assertEqual(data["nonrace_session_count"],5)
            with redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit):
                    main(argv[:-3])
            identical=root/"duplicate-race.json"
            identical.write_bytes(captures["race_a"].read_bytes())
            duplicate=list(argv)
            duplicate[3]=str(identical)
            with redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit):
                    main(duplicate)
            with redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit):
                    main(argv+["--control","hud",str(captures["hud"])])

if __name__ == "__main__":
    unittest.main()
