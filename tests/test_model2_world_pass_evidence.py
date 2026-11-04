import copy
import unittest
from tools.model2_world_pass_evidence import compare_sessions


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
        "original_draw_phase_missing_witnesses": 0,
        "original_draw_phase_inconsistent_witnesses": 0,
        "original_draw_ordinal_min": 12,
        "original_draw_ordinal_max": 20,
        "distinct_original_draw_ordinals": 3,
        "original_present_failed_witnesses": 0,
        "original_present_missing_witnesses": 0,
        "distinct_same_witness_joint_present_frame_count": count,
        "distinct_context_present_frame_count": count,
        "original_source_vertex_pipeline_status": ["supported"],
        "original_source_fill_status": ["solid"],
        "original_source_depth_cull_status": ["supported"],
        "same_witness_joint_cpu_matrix_commands": [
            [cpu, "0x010f0068", "0x12345678", "0x00000120"],
            [cpu, "0x010f00a8", "0x87654321", "0x00000121"],
        ],
    }


def report(*sources):
    return {
        "schema": 1,
        "source_kind": "real_d3d9_command_boundary_readonly_metadata",
        "world_pass_proof": "NOT_ESTABLISHED",
        "hmd_runtime_validation": "UNTESTED",
        "sources": list(sources),
    }


class OriginalWorldPassEvidenceTest(unittest.TestCase):
    def test_cross_phase_conservative_candidate_only(self):
        a, b = witness(), witness()
        # Same CPU caller, different matrix/command fingerprints per
        # independent run; matrices can change while a car/camera moves.
        b["same_witness_joint_cpu_matrix_commands"][0][2] = "0xaaaaaaaa"
        got = compare_sessions([report(a), report(b)], [report()])
        self.assertEqual(len(got["candidates"]), 1)
        self.assertFalse(got["adapter_auto_enable"])
        self.assertFalse(got["candidates"][0]["world_pass_authorized"])
        self.assertEqual(got["candidates"][0]
                         ["per_race_original_draw_ordinal_ranges"],
                         [[12, 20], [12, 20]])
        self.assertEqual(got["world_pass_proof"], "NOT_ESTABLISHED")
        self.assertEqual(got["hmd_runtime_validation"], "UNTESTED")

    def test_menu_reuse_blocks_promotion_even_if_joined_only_in_race(self):
        candidate = witness()
        neg = {"draw_family": candidate["draw_family"],
               "caller_rva_hex": candidate["caller_rva_hex"],
               "caller_is_in_executable": True}
        result = compare_sessions([report(candidate), report(candidate)],
                                  [report(neg)])
        self.assertFalse(result["candidates"])
        self.assertEqual(result["rejected"][0]["reason"],
                         "caller_also_used_outside_race")

    def test_missing_or_unsupported_same_witness_metadata_fails_closed(self):
        for damaged in (
            {"distinct_projected_material_present_frame_count": 1},
            {"distinct_same_witness_joint_present_frame_count": 0},
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
                                       [report()])
                self.assertEqual(got["candidates"], [])
                self.assertEqual(len(got["rejected"]), 1)

    def test_original_hresult_failure_missing_or_wrong_frame_blocks_candidates(self):
        for damaged in (
            {"distinct_successful_projected_material_present_frame_count": 0},
            {"original_draw_failure_witnesses": 1},
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
        ):
            with self.subTest(damaged=damaged):
                source = witness()
                source.update(damaged)
                result = compare_sessions([report(source), report(witness())],
                                          [report()])
                self.assertEqual(result["candidates"], [])
                self.assertEqual(result["rejected"][0]["reason"],
                                 "missing_same_draw_matrix_material_or_render_evidence")

    def test_cpu_caller_drift_and_mixed_projector_fail_closed(self):
        got = compare_sessions(
            [report(witness()), report(witness("0x004bb500"))],
            [report()])
        self.assertEqual(got["rejected"][0]["reason"],
                         "conflicting_original_cpu_projector_callers")
        mixed = witness()
        mixed["same_witness_joint_cpu_matrix_commands"][1][0] = "0x004bb500"
        got = compare_sessions([report(witness()), report(mixed)], [report()])
        self.assertFalse(got["candidates"])

    def test_missing_race_and_unknown_callers(self):
        got = compare_sessions([report(witness()), report()], [report()])
        self.assertEqual(got["rejected"][0]["reason"],
                         "missing_in_race_capture")
        unknown = witness()
        unknown["caller_rva_hex"] = "0x00000000"
        unknown["caller_is_in_executable"] = False
        got = compare_sessions([report(unknown), report(unknown)], [report()])
        self.assertEqual(got["rejected"][0]["reason"], "unknown_gpu_caller")

    def test_spoofed_original_caller_and_malformed_states_rejected(self):
        bad = witness()
        bad["caller_is_in_executable"] = False
        with self.assertRaises(ValueError):
            compare_sessions([report(bad), report(witness())], [report()])
        bad = witness()
        bad["original_source_fill_status"] = [["solid"]]
        got = compare_sessions([report(bad), report(witness())], [report()])
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
                    compare_sessions([bad, good], [report()])

    def test_duplicates_and_insufficient_phases_rejected(self):
        with self.assertRaises(ValueError):
            compare_sessions([report(witness())], [report()])
        with self.assertRaises(ValueError):
            compare_sessions([report(witness(), witness()), report(witness())],
                             [report()])


if __name__ == "__main__":
    unittest.main()
