"""Genuine original UP and physical Present outcome cross-correlation tests."""
import csv
import tempfile
import unittest
from pathlib import Path
from tools.model2_up_draw_outcomes import audit_csv, main_depth_source

def pack(high, low):
    return (high << 32) | low

def event(name, witness, a1, a2, a3):
    return (name, witness, a1, a2, a3)

class OriginalUPPhysicalOutcomeTest(unittest.TestCase):
    def report(self, events):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "private_source.csv"
            with path.open("w", newline="", encoding="utf-8") as handle:
                writer = csv.writer(handle)
                writer.writerow(["seq", "qpc", "thread", "event",
                                 "a0", "a1", "a2", "a3"])
                for index, row in enumerate(events, 1):
                    writer.writerow([index, index * 100, 7, *row])
            return audit_csv(path)

    def projected(self, witness=21, frame=7, caller=0x4321):
        return [
            event("model2_d3d9_source_pass_context", witness,
                  frame, 0xEFD, pack(640, 480)),
            event("model2_d3d9_up_source_fvf", witness,
                  0x144, 24, frame),
            event("model2_d3d9_up_meta", witness,
                  frame, caller, pack(3, 24)),
            event("model2_d3d9_cpu_up_join_batch", witness,
                  pack(2, 2), pack(0, 0), 0xABCD),
            event("model2_d3d9_cpu_up_matrix_identity", witness,
                  0x1008, 0, 0xABCD),
            event("model2_d3d9_cpu_up_matrix_content", witness,
                  0x12345678, 0, 0x1008),
            event("model2_d3d9_cpu_up_geometry_command", witness,
                  0x120, 0, 0x12345678),
            event("model2_d3d9_source_scene_phase", witness,
                  frame, 1, 1),
            event("model2_d3d9_source_draw_phase", witness,
                  frame, 12, pack(caller, 3)),
            event("model2_d3d9_up_original_result", witness,
                  0, caller, pack(frame, 3)),
        ]

    def test_same_source_fixed_function_lineage_for_up_present(self):
        fixed=event("model2_d3d9_source_fixed_function_lineage",
                    21,7,pack(1,2),pack(3,4))
        complete=self.report(self.projected()+[fixed,
            event("model2_d3d9_original_present_result",7,0,1,0)])
        self.assertEqual(complete["counts"]["original_fixed_function_lineage_witness"],1)
        self.assertEqual(complete["counts"]["same_witness_cpu_camera_present_candidate"],1)
        for bad in ((0,pack(1,2),pack(3,4)),
                    (7,pack(1_000_001,2),pack(3,4)),
                    (7,pack(1,2),pack(3,1_000_001))):
            with self.assertRaisesRegex(ValueError,"fixed-function"):
                self.report(self.projected()+[
                    event("model2_d3d9_source_fixed_function_lineage",21,*bad),
                    event("model2_d3d9_original_present_result",7,0,1,0)])
        wrong=self.report(self.projected()+[
            event("model2_d3d9_source_fixed_function_lineage",
                  21,8,pack(1,2),pack(3,4)),
            event("model2_d3d9_original_present_result",7,0,1,0)])
        self.assertEqual(wrong["counts"]["source_identity_or_frame_mismatch"],1)
        self.assertNotIn("same_witness_cpu_camera_present_candidate",wrong["counts"])
        with self.assertRaisesRegex(ValueError,"fixed-function"):
            self.report(self.projected()+[fixed,fixed])

    def test_real_original_up_successful_draw_and_present_with_stable_cpu_join(self):
        events = self.projected() + [
            event("model2_d3d9_original_present_result", 7, 0, 1, 0),
        ]
        got = self.report(events)
        self.assertEqual(
            got["counts"]["original_source_draw_and_present_succeeded"], 1)
        self.assertEqual(
            got["counts"]["same_witness_cpu_camera_present_candidate"], 1)
        self.assertEqual(got["counts"]["physical_draw_phase_authenticated"], 1)
        self.assertEqual(got["counts"]["original_scene_phase_attested"], 1)
        self.assertEqual(got["examples"][0]["original_scene_ordinal"], 1)
        self.assertEqual(got["examples"][0]["original_draw_ordinal"], 12)
        self.assertEqual(got["physically_presented_same_witness_camera_callers"][0]
                         ["first_original_draw_ordinal"], 12)
        self.assertEqual(got["world_pass_proof"], "NOT_ESTABLISHED")
        self.assertFalse(got["adapter_auto_enable"])
        self.assertEqual(
            got["physically_presented_same_witness_camera_callers"][0]
               ["distinct_original_present_frames"], 1)
        self.assertFalse(
            got["physically_presented_same_witness_camera_callers"][0]
               ["world_pass_authorized"])

    def test_unsampled_original_draw_failure_poisoned_present_and_recovery(self):
        # One sampled UP succeeds, another original source Draw in the same
        # physical Present fails without a sampled witness.
        events = self.projected() + [
            event("model2_d3d9_original_draw_failed",
                  7, 2, 0x8876086C, 0x8888),
            event("model2_d3d9_original_present_result", 7, 0, 1, 0),
        ]
        got = self.report(events)
        self.assertEqual(got["counts"]["other_original_draw_failed"], 1)
        self.assertNotIn("same_witness_cpu_camera_present_candidate",
                         got["counts"])
        for bad in (0, 5):
            invalid = list(events)
            invalid[-2] = event("model2_d3d9_original_draw_failed",
                                7, bad, 0x8876086C, 0x8888)
            with self.assertRaisesRegex(ValueError, "invalid original Draw failure"):
                self.report(invalid)
        invalid = list(events)
        invalid[-2] = event("model2_d3d9_original_draw_failed",
                            7, 2, 0, 0x8888)
        with self.assertRaisesRegex(ValueError, "invalid original Draw failure"):
            self.report(invalid)
        # A later physical frame can recover with an independent source.
        recovered = self.report(
            events + self.projected(witness=22, frame=8) +
            [event("model2_d3d9_original_present_result", 8, 0, 1, 0)])
        self.assertEqual(
            recovered["counts"]["same_witness_cpu_camera_present_candidate"], 1)

    def test_physical_present_failure_and_unknown_source_never_candidate(self):
        events = self.projected() + [
            event("model2_d3d9_original_present_result",
                  7, 0x88760868, 1, 0),
        ]
        got = self.report(events)
        self.assertEqual(got["counts"]["physical_original_present_failed"], 1)
        self.assertNotIn("same_witness_cpu_camera_present_candidate",
                         got["counts"])
        # Missing original Present in a truncated private CSV fails closed.
        got = self.report(self.projected())
        self.assertEqual(got["counts"]["physical_original_present_missing"], 1)

    def test_original_draw_hresult_failure_even_with_real_present(self):
        events = self.projected()
        events[-1] = event("model2_d3d9_up_original_result",
                           21, 0x8876086C, 0x4321, pack(7, 3))
        events.append(event("model2_d3d9_original_present_result",
                            7, 0, 1, 0))
        got = self.report(events)
        self.assertEqual(got["counts"]["original_draw_failed"], 1)
        self.assertNotIn("same_witness_cpu_camera_present_candidate",
                         got["counts"])

    def test_source_mismatch_and_late_source_failure_are_rejected(self):
        events = self.projected()
        events[-1] = event("model2_d3d9_up_original_result",
                           21, 0, 0x9999, pack(7, 3))
        events.append(event("model2_d3d9_original_present_result",
                            7, 0, 1, 0))
        got = self.report(events)
        self.assertEqual(got["counts"]["source_identity_or_frame_mismatch"], 1)
        self.assertFalse(got["physically_presented_same_witness_camera_callers"])

    def test_same_witness_fvf_stride_conflict_cannot_fake_world_candidate(self):
        events = self.projected()
        events[1] = event("model2_d3d9_up_source_fvf", 21,
                          0x144, 32, 7)  # source stride contradicts UP meta
        events.append(event("model2_d3d9_original_present_result",
                            7, 0, 1, 0))
        got = self.report(events)
        self.assertEqual(
            got["counts"]["source_identity_or_frame_mismatch"], 1)
        self.assertFalse(got["physically_presented_same_witness_camera_callers"])

    def test_original_fixed_world_xyz_and_shader_up_are_distinct(self):
        events = [
            event("model2_d3d9_source_pass_context", 31,
                  7, 0xEFD, pack(640, 480)),
            event("model2_d3d9_up_source_fvf", 31, 0x142, 24, 7),
            event("model2_world_up_candidate", 31, 0x1111,
                  0x142, pack(4, 24)),
            event("model2_d3d9_source_scene_phase", 31, 7, 1, 1),
            event("model2_d3d9_source_draw_phase", 31,
                  7, 25, pack(0x1111, 4)),
            event("model2_d3d9_up_original_result", 31, 0,
                  0x1111, pack(7, 4)),
            event("model2_d3d9_original_present_result", 7, 0, 1, 0),
            event("model2_d3d9_source_pass_context", 32,
                  8, 0xEFD, pack(640, 480)),
            event("model2_d3d9_up_layout", 32, 0x2222,
                  pack(3, 32), pack(2, 2)),
            event("model2_d3d9_up_layout_frame", 32, 8, 0x2222, 0),
            event("model2_d3d9_cpu_shader_up_join_batch", 32,
                  pack(2, 2), pack(0, 0), 0x1234),
            event("model2_d3d9_cpu_shader_matrix_identity", 32,
                  0x1010, 0, 0x1234),
            event("model2_d3d9_cpu_shader_matrix_content", 32,
                  0x12345678, 0, 0x1010),
            event("model2_d3d9_cpu_shader_geometry_command", 32,
                  0x44, 0, 0x12345678),
            event("model2_d3d9_source_scene_phase", 32, 8, 2, 1),
            event("model2_d3d9_source_draw_phase", 32,
                  8, 26, pack(0x2222, 3)),
            event("model2_d3d9_up_original_result", 32, 0,
                  0x2222, pack(8, 3)),
            event("model2_d3d9_original_present_result", 8, 0, 1, 0),
        ]
        got = self.report(events)
        self.assertEqual(
            got["counts"]["original_source_draw_and_present_succeeded"], 2)
        self.assertEqual(got["counts"]["source_kind_fixed_xyz"], 1)
        self.assertEqual(got["counts"]["source_kind_shader_decl"], 1)
        self.assertEqual(
            got["counts"]["same_witness_cpu_camera_present_candidate"], 1)
        self.assertEqual(
            got["physically_presented_same_witness_camera_callers"][0]
               ["original_source_kind"], "shader_decl")

    def test_no_main_depth_and_changing_geometry_do_not_establish_join(self):
        events = self.projected()
        events[0] = event("model2_d3d9_source_pass_context", 21,
                           7, 0xEFA, pack(640, 480))  # aux/unknown
        events.append(event("model2_d3d9_original_present_result",
                            7, 0, 1, 0))
        got = self.report(events)
        self.assertNotIn("same_witness_cpu_camera_present_candidate",
                         got["counts"])
        self.assertTrue(main_depth_source(0xEFD))
        self.assertFalse(main_depth_source(0xEFA))

    def test_reject_duplicate_hresults_and_bad_physical_sample_count(self):
        events = self.projected()
        with self.assertRaisesRegex(ValueError, "duplicate original UP Draw result"):
            self.report(events + [events[-1]])
        with self.assertRaisesRegex(ValueError, "more UP results"):
            self.report(events + [
                event("model2_d3d9_up_source_fvf", 22, 0x144, 24, 7),
                event("model2_d3d9_up_meta", 22, 7, 0x4444, pack(3, 24)),
                event("model2_d3d9_source_draw_phase", 22,
                      7, 13, pack(0x4444, 3)),
                event("model2_d3d9_up_original_result", 22, 0,
                      0x4444, pack(7, 3)),
                event("model2_d3d9_original_present_result", 7, 0, 1, 0),
            ])
        with self.assertRaisesRegex(ValueError, "duplicate physical original Present"):
            self.report(events + [
                event("model2_d3d9_original_present_result", 7, 0, 1, 0),
                event("model2_d3d9_original_present_result", 7, 0, 1, 0),
            ])

    def test_late_failed_original_scene_revokes_previously_successful_draw(self):
        events = self.projected() + [
            # Original Draw succeeded, but the same genuine original scene
            # fails after that Draw and before physical original Present.
            event("model2_d3d9_original_scene_failed", 7,
                  2, 0x8876086C, 0x4321),
            event("model2_d3d9_original_present_result", 7, 0, 1, 0),
        ]
        got = self.report(events)
        self.assertEqual(
            got["counts"]["original_scene_transition_failed"], 1)
        self.assertNotIn(
            "original_source_draw_and_present_succeeded", got["counts"])
        self.assertFalse(
            got["physically_presented_same_witness_camera_callers"])
        with self.assertRaisesRegex(ValueError, "invalid original scene failure"):
            self.report(self.projected() + [
                event("model2_d3d9_original_scene_failed", 7,
                      2, 0, 0x4321)])
        # The following physical Present may re-establish an independent
        # actual scene. Old failed frame must not contaminate fresh source.
        next_frame = self.projected(witness=22,frame=8)
        got = self.report(events + next_frame + [
            event("model2_d3d9_original_present_result", 8, 0, 1, 0)])
        self.assertEqual(
            got["counts"]["original_scene_transition_failed"], 1)
        self.assertEqual(
            got["counts"]["original_source_draw_and_present_succeeded"], 1)
        self.assertEqual(
            got["counts"]["same_witness_cpu_camera_present_candidate"], 1)

    def test_failed_original_clear_is_physical_frame_poison_even_after_draw(self):
        events=self.projected() + [
            event("model2_d3d9_original_clear_failed",7,
                  3,0x8876086C,0x4321),
            event("model2_d3d9_original_present_result",7,0,1,0),
        ]
        got=self.report(events)
        self.assertEqual(got["counts"]["original_clear_failed"],1)
        self.assertNotIn("same_witness_cpu_camera_present_candidate",got["counts"])
        self.assertFalse(got["physically_presented_same_witness_camera_callers"])
        bad=list(events)
        bad[-2]=event("model2_d3d9_original_clear_failed",7,3,0,0x4321)
        with self.assertRaisesRegex(ValueError,"invalid original Clear failure"):
            self.report(bad)
        bad[-2]=event("model2_d3d9_original_clear_failed",7,
                      3,0x8876086C,0)
        with self.assertRaisesRegex(ValueError,"invalid original Clear failure"):
            self.report(bad)
        recovered=self.report(events+self.projected(witness=22,frame=8)+[
            event("model2_d3d9_original_present_result",8,0,1,0)])
        self.assertEqual(recovered["counts"]["original_clear_failed"],1)
        self.assertEqual(recovered["counts"]["original_source_draw_and_present_succeeded"],1)

    def test_real_original_surface_failure_blocks_same_witness_up(self):
        for method in range(1,26):
            events=self.projected()+[
                event("model2_d3d9_original_surface_failed",
                      7,method,0x8876086C,0x4321),
                event("model2_d3d9_original_present_result",7,0,1,0),
            ]
            got=self.report(events)
            self.assertEqual(got["counts"]["original_surface_failed"],1)
            self.assertNotIn("same_witness_cpu_camera_present_candidate",
                             got["counts"])
            recovered=self.report(events+self.projected(witness=22,frame=8)+[
                event("model2_d3d9_original_present_result",8,0,1,0)])
            self.assertEqual(
                recovered["counts"]["same_witness_cpu_camera_present_candidate"],1)
        drift=self.projected()+[
            event("model2_d3d9_original_lineage_drift",7,3,0x4321,5),
            event("model2_d3d9_original_present_result",7,0,1,0),
        ]
        result=self.report(drift)
        self.assertEqual(result["counts"]["original_surface_failed"],1)
        self.assertNotIn("same_witness_cpu_camera_present_candidate",result["counts"])
        for bad in ((5,0x4321,5),(3,0,5),(3,0x4321,0)):
            with self.assertRaisesRegex(ValueError,"invalid original surface failure"):
                self.report(self.projected()+[
                    event("model2_d3d9_original_lineage_drift",7,*bad)])
        for active_mask in (1,2,3,0x80000000):
            frame=self.projected()+[
                event("model2_d3d9_original_instancing_active",7,active_mask,0,0),
                event("model2_d3d9_original_present_result",7,0,1,0)]
            result=self.report(frame)
            self.assertEqual(result["counts"]["original_surface_failed"],1)
            self.assertNotIn("same_witness_cpu_camera_present_candidate",result["counts"])
        for bad in ((7,0,0,0),(0,1,0,0),(7,1,1,0),(7,1,0,1)):
            with self.assertRaisesRegex(ValueError,"invalid original active instancing source"):
                self.report(self.projected()+[
                    event("model2_d3d9_original_instancing_active",*bad)])
        for frequency in (0,2,0x40000001,0x80000001):
            blocked=self.projected()+[
                event("model2_d3d9_original_instancing_unsupported",7,0,0x4321,frequency),
                event("model2_d3d9_original_present_result",7,0,1,0),
            ]
            result=self.report(blocked)
            self.assertEqual(result["counts"]["original_surface_failed"],1)
            self.assertNotIn("same_witness_cpu_camera_present_candidate",result["counts"])
        for bad in ((7,0,0x4321,1),(7,0,0,2),
                    (0,0,0x4321,2),(7,0,0x4321,0x100000000)):
            with self.assertRaisesRegex(ValueError,"invalid original instancing source"):
                self.report(self.projected()+[
                    event("model2_d3d9_original_instancing_unsupported",*bad)])
        for stage in (1,2):
            blocked=self.projected()+[
                event("model2_d3d9_original_stateblock_mutation",7,stage,0x4321,0),
                event("model2_d3d9_original_present_result",7,0,1,0),
            ]
            result=self.report(blocked)
            self.assertEqual(result["counts"]["original_surface_failed"],1)
            self.assertNotIn("same_witness_cpu_camera_present_candidate",result["counts"])
        for bad in ((7,0,0x4321,0),(7,3,0x4321,0),
                    (7,1,0,0),(7,1,0x4321,0x100000000)):
            with self.assertRaisesRegex(ValueError,"invalid original stateblock mutation"):
                self.report(self.projected()+[
                    event("model2_d3d9_original_stateblock_mutation",*bad)])
        for bad in ((0,0x8876086C,0x4321),
                    (26,0x8876086C,0x4321),
                    (2,0,0x4321),
                    (3,0x8876086C,0)):
            events=self.projected()+[
                event("model2_d3d9_original_surface_failed",7,*bad)]
            with self.assertRaisesRegex(ValueError,"invalid original surface failure"):
                self.report(events)

    def test_missing_or_inactive_original_scene_cannot_claim_camera(self):
        original = self.projected()
        got = self.report([e for e in original
                           if e[0] != "model2_d3d9_source_scene_phase"] +
                          [event("model2_d3d9_original_present_result",
                                 7, 0, 1, 0)])
        self.assertEqual(got["counts"]["original_scene_phase_missing"], 1)
        self.assertFalse(got["physically_presented_same_witness_camera_callers"])
        inactive = list(original)
        inactive[-3] = event("model2_d3d9_source_scene_phase", 21, 7, 1, 0)
        got = self.report(inactive + [
            event("model2_d3d9_original_present_result", 7, 0, 1, 0)])
        self.assertEqual(got["counts"]["original_scene_phase_inactive"], 1)
        self.assertFalse(got["physically_presented_same_witness_camera_callers"])
        wrong = list(original)
        wrong[-3] = event("model2_d3d9_source_scene_phase", 21, 8, 1, 1)
        got = self.report(wrong + [
            event("model2_d3d9_original_present_result", 7, 0, 1, 0)])
        self.assertEqual(got["counts"]["source_identity_or_frame_mismatch"], 1)
        self.assertFalse(got["physically_presented_same_witness_camera_callers"])

    def test_missing_original_phase_cannot_claim_phase_attested_camera(self):
        events = [r for r in self.projected()
                  if r[0] != "model2_d3d9_source_draw_phase"]
        got = self.report(events + [
            event("model2_d3d9_original_present_result", 7, 0, 1, 0)])
        self.assertEqual(got["counts"]["physical_draw_phase_missing"], 1)
        self.assertEqual(got["counts"]["original_source_draw_and_present_succeeded"], 1)
        self.assertFalse(got["physically_presented_same_witness_camera_callers"])

    def test_wrong_frame_family_or_caller_phase_cannot_qualify(self):
        for bad in [(8, 12, pack(0x4321, 3)),
                    (7, 12, pack(0x4321, 4)),
                    (7, 12, pack(0x9999, 3))]:
            with self.subTest(bad=bad):
                events = self.projected()
                events[-2] = event("model2_d3d9_source_draw_phase",
                                   21, *bad)
                got = self.report(events + [
                    event("model2_d3d9_original_present_result", 7, 0, 1, 0)])
                self.assertEqual(
                    got["counts"]["source_identity_or_frame_mismatch"], 1)
                self.assertFalse(
                    got["physically_presented_same_witness_camera_callers"])

    def test_cross_family_same_physical_phase_ordinal_rejected(self):
        events = self.projected()
        events.extend([
            event("model2_d3d9_up_meta", 22, 7, 0x4444, pack(3, 24)),
            event("model2_d3d9_source_draw_phase", 22,
                  7, 12, pack(0x4444, 3)),
        ])
        with self.assertRaisesRegex(ValueError, "duplicate original Draw phase"):
            self.report(events)

    def test_unrelated_vb_source_context_does_not_become_up_evidence(self):
        got = self.report([
            event("model2_d3d9_source_pass_context", 300, 7,
                  0xEFD, pack(640, 480)),
            event("model2_d3d9_original_present_result", 7, 0, 2, 0),
        ])
        self.assertFalse(got["counts"])
        self.assertFalse(got["physically_presented_same_witness_camera_callers"])

if __name__ == "__main__":
    unittest.main()
