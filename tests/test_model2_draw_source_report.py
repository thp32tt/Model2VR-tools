"""Regression for read-only D3D9 source-caller CSV analysis."""
import csv
import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "model2_draw_report", ROOT / "tools" / "model2_draw_source_report.py")
REPORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REPORT)

def pack(upper, lower):
    return (upper << 32) | lower

class SourceAuditTests(unittest.TestCase):
    def write_log(self, path, events):
        with path.open("w", encoding="utf-8", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["seq", "qpc", "thread", "event", "a0", "a1", "a2", "a3"])
            for i, event in enumerate(events, 1):
                writer.writerow([i, 1000 + i, 4, *event])

    def test_scene_draw_streamed_every_present_with_bounded_control_samples(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/"long_private_original_source.csv"
            frames=512
            caller=0x1234
            lo,hi=REPORT._original_caller_exact_bloom({caller})
            events=[("model2_d3d9_original_draw_caller_first",
                     1,caller,pack(1,1),0)]
            for frame in range(1,frames+1):
                if frame==1 or frame%8==0:
                    events.append(("model2_d3d9_original_control_present_phase",
                                   frame,0,pack(1,0),0))
                events.extend((
                    ("model2_d3d9_original_all_family_caller_bloom",
                     frame,lo,hi,0),
                    ("model2_d3d9_original_caller_inventory",frame,1,0,0),
                    ("model2_d3d9_original_capture_run",
                     frame,0xfedcba9876543210,0,0),
                    ("model2_d3d9_original_scene_draw_coverage",
                     frame,pack(1,1),pack(1,0),pack(0,0)),
                    ("model2_d3d9_original_camera_com_lifetime",
                     frame,0,1,3),
                    ("model2_d3d9_original_camera_com_families",
                     frame,1,1,1),
                    ("model2_d3d9_original_draw_target_coverage",
                     frame,pack(1,0),pack(0,0),pack(0,0)),
                ))
            self.write_log(path,events)
            data=REPORT.audit_csv(path)
            scene=data["original_physical_scene_draw_coverage"]
            self.assertEqual(scene["physical_present_count"],512)
            self.assertEqual(scene["original_begin_scene_count"],512)
            self.assertEqual(scene["original_successful_in_scene_draw_count"],512)
            self.assertEqual(scene["trusted_scene_drawn_present_count"],512)
            self.assertTrue(scene["all_original_scene_draws_trusted"])
            camera=data["original_physical_camera_com_lifetime"]
            self.assertEqual(camera["physical_present_count"],512)
            self.assertEqual(camera["original_com_draw_count"],512)
            self.assertEqual(camera["original_camera_write_count"],1536)
            self.assertEqual(camera["original_camera_state_write_count"],512)
            self.assertEqual(camera["original_pixel_program_write_count"],512)
            self.assertEqual(camera["original_vertex_layout_write_count"],512)
            self.assertTrue(camera["all_original_camera_com_lifetimes_trusted"])
            target=data["original_physical_draw_target_coverage"]
            self.assertEqual(target["physical_present_count"],512)
            self.assertEqual(target["original_backbuffer_draw_count"],512)
            self.assertEqual(target["trusted_backbuffer_present_count"],512)
            self.assertTrue(target["all_original_draw_targets_trusted"])
            self.assertEqual(data["original_control_present_sample_count"],65)
            self.assertEqual(data["original_control_four_family_draw_totals"][
                "DrawPrimitive"],65)
            # A late unsampled frame change must poison the *whole* capture.
            late=list(events)
            target=next(i for i, row in enumerate(late)
                        if row[0]=="model2_d3d9_original_scene_draw_coverage"
                        and row[1]==frames-1)
            late[target]=("model2_d3d9_original_scene_draw_coverage",
                          frames-1,pack(1,1),pack(0,1),0)
            self.write_log(path,late)
            poisoned=REPORT.audit_csv(path)["original_physical_scene_draw_coverage"]
            self.assertFalse(poisoned["all_original_scene_draws_trusted"])
            self.assertEqual(poisoned["original_successful_untrusted_scene_draw_count"],1)
            # Every x86 real COM write straddling Draw/Present poisons source.
            camera_index=next(i for i,row in enumerate(events)
                if row[0]=="model2_d3d9_original_camera_com_lifetime"
                and row[1]==frames-1)
            for flag in (1,2,4,8,16,32,64,127,128,256,512,1023):
                bad=list(events)
                bad[camera_index]=(
                    "model2_d3d9_original_camera_com_lifetime",
                    frames-1,flag,1,3)
                self.write_log(path,bad)
                state=REPORT.audit_csv(path)["original_physical_camera_com_lifetime"]
                self.assertFalse(state["all_original_camera_com_lifetimes_trusted"])
                self.assertEqual(state["flagged_present_count"],1)
            for frame,flags,draws in (
                (frames-1,1024,1),(frames-1,0,0),(frames,0,1)):
                bad=list(events)
                bad[camera_index]=(
                    "model2_d3d9_original_camera_com_lifetime",
                    frame,flags,draws,3)
                self.write_log(path,bad)
                with self.assertRaisesRegex(
                    ValueError,"invalid original camera COM lifetime"):
                    REPORT.audit_csv(path)
            # A shader-only physical Present cannot impersonate a real
            # original view/projection Write with the same total COM count.
            family_index=next(i for i,row in enumerate(events)
                if row[0]=="model2_d3d9_original_camera_com_families"
                and row[1]==frames-1)
            for frame,camera,pixel,layout in (
                (frames-1,0,0,0),(frames-1,1,1,2),
                (frames-1,0,1,1),(frames,1,1,1)):
                bad=list(events)
                bad[family_index]=(
                    "model2_d3d9_original_camera_com_families",
                    frame,camera,pixel,layout)
                self.write_log(path,bad)
                with self.assertRaisesRegex(
                    ValueError,"invalid original camera COM family"):
                    REPORT.audit_csv(path)
            # An incomplete family stream cannot attest the same physical
            # original Present even if the aggregate write count is correct.
            missing=[row for row in events if not (
                row[0]=="model2_d3d9_original_camera_com_families"
                and row[1]==frames)]
            self.write_log(path,missing)
            with self.assertRaisesRegex(
                ValueError,"orphan original camera COM lifetime evidence"):
                REPORT.audit_csv(path)
            # Native RT0/depth event is a separate physical witness. Its
            # count/epoch must agree with the ORIGINAL unsampled Draw total.
            target_index=next(i for i,row in enumerate(events)
                if row[0]=="model2_d3d9_original_draw_target_coverage"
                and row[1]==frames-1)
            for flags,depth in ((4,0),(32,0),(64,0),(8,1),
                                (512,0),(1024,0),(1536,0),
                                (2048,0),(3072,0),(4096,0),(8192,0),(12288,0)):
                bad=list(events)
                bad[target_index]=(
                    "model2_d3d9_original_draw_target_coverage",
                    frames-1,pack(1,0),pack(0,0),pack(depth,flags))
                self.write_log(path,bad)
                state=REPORT.audit_csv(path)[
                    "original_physical_draw_target_coverage"]
                self.assertFalse(state["all_original_draw_targets_trusted"])
                self.assertEqual(state["flagged_present_count"],1)
            # Authentic auxiliary RT0 Draws are never direct-eye evidence.
            offscreen=list(events)
            offscreen[target_index]=(
                "model2_d3d9_original_draw_target_coverage",
                frames-1,pack(0,1),pack(0,0),pack(0,4096))
            self.write_log(path,offscreen)
            source=REPORT.audit_csv(path)["original_physical_draw_target_coverage"]
            self.assertEqual(source["original_auxiliary_draw_count"],1)
            self.assertEqual(source["flagged_present_count"],1)
            self.assertFalse(source["all_original_draw_targets_trusted"])
            offscreen[target_index]=(
                "model2_d3d9_original_draw_target_coverage",
                frames-1,pack(0,1),pack(0,0),pack(0,0))
            self.write_log(path,offscreen)
            with self.assertRaisesRegex(
                    ValueError,"invalid original physical target Draw coverage"):
                REPORT.audit_csv(path)
            # A failed/omitted physical Draw cannot be borrowed from the
            # adjacent valid frame by silently editing just its target row.
            for bad_event in (
                ("model2_d3d9_original_draw_target_coverage",
                 frames-1,pack(0,0),pack(0,0),pack(0,0)),
                ("model2_d3d9_original_draw_target_coverage",
                 frames-1,pack(1,0),pack(0,0),pack(0,16384)),
                ("model2_d3d9_original_draw_target_coverage",
                 frames-1,pack(1,0),pack(0,0),pack(1,0)),
            ):
                bad=list(events)
                bad[target_index]=bad_event
                self.write_log(path,bad)
                with self.assertRaisesRegex(
                    ValueError,"invalid original physical target Draw coverage"):
                    REPORT.audit_csv(path)
            # If the genuine original source cuts two COM epochs, the
            # 256 count-mismatch bit is a valid negative evidence record.
            crossed=list(events)
            crossed[target_index]=(
                "model2_d3d9_original_draw_target_coverage",
                frames-1,pack(0,0),pack(0,0),pack(0,256))
            self.write_log(path,crossed)
            self.assertFalse(REPORT.audit_csv(path)[
                "original_physical_draw_target_coverage"][
                "all_original_draw_targets_trusted"])
            # Every real Present includes unsampled coverage. A COM call
            # spanning Present poisons that physical frame and the next;
            # neither flag is unknown metadata nor trusted world evidence.
            for flags in (128, 256, 384):
                crossing=list(events)
                crossing[target]=(
                    "model2_d3d9_original_scene_draw_coverage",
                    frames-1,pack(1,1),pack(1,0),pack(0,flags))
                self.write_log(path,crossing)
                coverage=REPORT.audit_csv(path)["original_physical_scene_draw_coverage"]
                self.assertFalse(coverage["all_original_scene_draws_trusted"])
                self.assertEqual(coverage["flagged_present_count"],1)
                self.assertEqual(coverage["trusted_scene_drawn_present_count"],frames-1)
            # A future/undefined bit cannot be silently treated as valid.
            crossing[target]=(
                "model2_d3d9_original_scene_draw_coverage",
                frames-1,pack(1,1),pack(1,0),pack(0,512))
            self.write_log(path,crossing)
            with self.assertRaisesRegex(ValueError,"invalid original physical Scene/Draw coverage"):
                REPORT.audit_csv(path)

    def test_original_unsampled_scene_draw_matches_original_physical_counts(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/"scene_coverage.csv"
            caller=0x1234
            lo,hi=REPORT._original_caller_exact_bloom({caller})
            rows=[
                ("model2_d3d9_original_draw_caller_first",
                 1,caller,pack(3,1),0),
                ("model2_d3d9_original_control_present_phase",1,0,
                 pack(0,0),pack(2,0)),
                ("model2_d3d9_original_all_family_caller_bloom",1,lo,hi,0),
                ("model2_d3d9_original_caller_inventory",1,1,0,0),
                ("model2_d3d9_original_capture_run",1,0x123456789abcdef0,0,0),
                ("model2_d3d9_original_scene_draw_coverage",1,
                 pack(1,1),pack(2,0),pack(0,0))
            ]
            self.write_log(path,rows)
            got=REPORT.audit_csv(path)
            cov=got["original_physical_scene_draw_coverage"]
            self.assertTrue(cov["all_original_scene_draws_trusted"])
            self.assertEqual(cov["trusted_scene_drawn_present_count"],1)
            self.assertEqual(cov["original_successful_in_scene_draw_count"],2)
            for row in (
                ("model2_d3d9_original_scene_draw_coverage",1,
                 pack(1,1),pack(1,0),pack(0,0)),
                ("model2_d3d9_original_scene_draw_coverage",1,
                 pack(1,1),pack(2,0),pack(0,1)),
            ):
                bad=rows.copy()
                bad[-1]=row
                self.write_log(path,bad)
                if row[-1]:
                    result=REPORT.audit_csv(path)
                    self.assertFalse(result["original_physical_scene_draw_coverage"][
                        "all_original_scene_draws_trusted"])
                else:
                    with self.assertRaisesRegex(ValueError,"Scene/Draw count mismatch"):
                        REPORT.audit_csv(path)
            self.write_log(path,rows[:-1])
            self.assertIsNone(REPORT.audit_csv(path)[
                "original_physical_scene_draw_coverage"])
            bad=rows.copy()
            bad[-1]=("model2_d3d9_original_scene_draw_coverage",2,
                     pack(1,1),pack(2,0),0)
            self.write_log(path,bad)
            with self.assertRaisesRegex(ValueError,"Scene/Draw coverage"):
                REPORT.audit_csv(path)

    def test_native_original_capture_run_contiguous_physical_presents(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"private_run.csv"
            lo,hi=REPORT._original_caller_exact_bloom({0x1234})
            common=[
                ("model2_d3d9_original_draw_caller_first",
                 1,0x1234,pack(1,1),0),
                ("model2_d3d9_original_control_present_phase",
                 1,0,pack(1,0),0),
                ("model2_d3d9_original_all_family_caller_bloom",
                 1,lo,hi,0),
                ("model2_d3d9_original_caller_inventory",1,1,0,0),
                ("model2_d3d9_original_capture_run",1,0x123456789abcdef0,0,0),
                ("model2_d3d9_original_all_family_caller_bloom",
                 2,lo,hi,0),
                ("model2_d3d9_original_caller_inventory",2,1,0,0),
                ("model2_d3d9_original_capture_run",2,0x123456789abcdef0,0,0),
            ]
            self.write_log(p,common)
            got=REPORT.audit_csv(p)
            self.assertEqual(got["original_capture_run"]["nonce_hex"],
                             "0x123456789abcdef0")
            self.assertEqual(got["original_capture_run"]["physical_present_count"],2)
            old=common[:4]+common[5:7]
            self.write_log(p,old)
            self.assertIsNone(REPORT.audit_csv(p)["original_capture_run"])
            for changed in (
                ("model2_d3d9_original_capture_run",2,0xabcdef,0,0),
                ("model2_d3d9_original_capture_run",3,0x123456789abcdef0,0,0),
                ("model2_d3d9_original_capture_run",2,0,0,0),
                ("model2_d3d9_original_capture_run",2,0x123456789abcdef0,1,0),
            ):
                with self.subTest(changed=changed):
                    damaged=common.copy()
                    damaged[-1]=changed
                    self.write_log(p,damaged)
                    with self.assertRaisesRegex(ValueError,"capture run provenance"):
                        REPORT.audit_csv(p)
            self.write_log(p,common[:-1])
            with self.assertRaisesRegex(ValueError,"orphan original capture run"):
                REPORT.audit_csv(p)

    def test_original_validate_device_multi_pass_cannot_claim_world(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"original_validate_device_private.csv"
            source=[
                ("model2_d3d9_vb_present_frame",501,7,1,0),
                ("model2_d3d9_vb_source",501,0x1234,pack(1,0x104),pack(4096,24)),
                ("model2_d3d9_source_draw_phase",501,7,12,pack(0x1234,1)),
                ("model2_d3d9_vb_original_result",501,0,0x1234,pack(7,1)),
            ]
            for caller,ordinal,reason in (
                    (0x4321,12,1),(0x4321,12,2),(0x4321,12,3),
                    (0,0,4),(0,0,5)):
                self.write_log(p,source+[
                    ("model2_d3d9_original_validation_rejected",
                     7,caller,ordinal,reason),
                    ("model2_d3d9_original_present_result",7,0,1,0)])
                row=REPORT.audit_csv(p)["sources"][0]
                self.assertEqual(row["original_surface_failure_witnesses"],1)
                self.assertFalse(row["world_pass_proven"])
            for invalid in (
                    (0,0x4321,12,1),(7,0,12,1),(7,0,12,2),
                    (7,0,12,3),(7,0x4321,12,4),(7,0,1,5),
                    (7,0x4321,10000001,2),(7,0,0,6)):
                self.write_log(p,source+[
                    ("model2_d3d9_original_validation_rejected",*invalid)])
                with self.assertRaisesRegex(ValueError,"ValidateDevice source rejection"):
                    REPORT.audit_csv(p)

    def test_original_query_issue_and_result_fail_closed_original_vb(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/"query_private_source.csv"
            source=[
                ("model2_d3d9_vb_present_frame",501,7,1,0),
                ("model2_d3d9_vb_source",501,0x1234,pack(1,0x104),pack(4096,24)),
                ("model2_d3d9_source_draw_phase",501,7,12,pack(0x1234,1)),
                ("model2_d3d9_vb_original_result",501,0,0x1234,pack(7,1)),
            ]
            clean=source+[("model2_d3d9_original_present_result",7,0,1,0)]
            self.write_log(path,clean)
            self.assertEqual(REPORT.audit_csv(path)["sources"][0][
                "original_surface_failure_witnesses"],0)
            for caller,ordinal,reason in ((0x4321,12,1),(0x4321,12,2),(0,0,3)):
                self.write_log(path,source+[
                    ("model2_d3d9_original_query_source_rejected",
                     7,caller,ordinal,reason),
                    ("model2_d3d9_original_present_result",7,0,1,0)])
                result=REPORT.audit_csv(path)["sources"][0]
                self.assertEqual(result["original_surface_failure_witnesses"],1)
                self.assertFalse(result["world_pass_proven"])
            for bad in ((0,0x4321,12,1),(7,0,12,1),(7,0,12,2),
                        (7,0x4321,12,3),(7,0,1,3),
                        (7,0x4321,10000001,2),(7,0x4321,0,4)):
                self.write_log(path,source+[
                    ("model2_d3d9_original_query_source_rejected",*bad)])
                with self.assertRaisesRegex(ValueError,"Query source rejection"):
                    REPORT.audit_csv(path)

    def test_real_original_display_dialog_gamma_revoke_same_present(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"private_display_source.csv"
            prefix=[
                ("model2_d3d9_vb_present_frame",501,17,1,0),
                ("model2_d3d9_vb_source",501,0x1234,pack(1,0x104),pack(4096,24)),
                ("model2_d3d9_source_draw_phase",501,17,12,pack(0x1234,1)),
                ("model2_d3d9_vb_original_result",501,0,0x1234,pack(17,1)),
            ]
            self.write_log(p,prefix+[(
                "model2_d3d9_original_present_result",17,0,1,0)])
            clean=REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(clean["original_surface_failure_witnesses"],0)
            for caller,ordinal,method in ((0x4321,12,1),(0x4321,12,2),
                                          (0,0,3),(0,0,4),(0,0,5)):
                self.write_log(p,prefix+[
                    ("model2_d3d9_original_presentation_control_rejected",
                     17,caller,ordinal,method),
                    ("model2_d3d9_original_present_result",17,0,1,0)])
                got=REPORT.audit_csv(p)["sources"][0]
                self.assertEqual(got["original_surface_failure_witnesses"],1)
                self.assertFalse(got["world_pass_proven"])
            for invalid in ((0,0x4321,12,1),(17,0,12,1),
                            (17,0x4321,10000001,2),(17,0x4321,12,5),
                            (17,0x4321,0,3),(17,0,1,4),(17,0x4321,0,5)):
                self.write_log(p,prefix+[
                    ("model2_d3d9_original_presentation_control_rejected",
                     *invalid)])
                with self.assertRaisesRegex(ValueError,"presentation control rejection"):
                    REPORT.audit_csv(p)

    def test_real_alternate_swap_chain_present_revokes_original_vb_source(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/"private_alternate_swap_chain.csv"
            source=[
                ("model2_d3d9_vb_present_frame",201,17,1,0),
                ("model2_d3d9_vb_source",201,0x1234,pack(1,0x104),pack(4096,24)),
                ("model2_d3d9_source_draw_phase",201,17,12,pack(0x1234,1)),
                ("model2_d3d9_vb_original_result",201,0,0x1234,pack(17,1)),
            ]
            for caller,reason in ((0x4321,1),(0,2),(0x4321,2),(0x4321,3)):
                self.write_log(path,source+[
                    ("model2_d3d9_original_swap_chain_source_rejected",
                     17,caller,reason,0),
                    ("model2_d3d9_original_present_result",17,0,1,0)])
                got=REPORT.audit_csv(path)["sources"][0]
                self.assertEqual(got["original_surface_failure_witnesses"],1)
                self.assertFalse(got["world_pass_proven"])
            for values in ((0,0x4321,1,0),(17,0,1,0),
                           (17,0x4321,0,0),(17,0x4321,4,0),
                           (17,0x4321,1,1),(17,0,3,0)):
                self.write_log(path,source+[
                    ("model2_d3d9_original_swap_chain_source_rejected",*values)])
                with self.assertRaisesRegex(ValueError,"SwapChain rejection"):
                    REPORT.audit_csv(path)

    def test_authentic_gpu_surface_lineage_is_same_physical_draw_only(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"private_gpu_surface.csv"
            frame=17
            rows=[
                ("model2_d3d9_vb_present_frame",201,frame,1,0),
                ("model2_d3d9_vb_source",201,0x1234,pack(1,0x104),pack(4096,24)),
                ("model2_d3d9_source_draw_phase",201,frame,12,pack(0x1234,1)),
                ("model2_d3d9_source_gpu_surface_lineage",201,frame,7,0),
                ("model2_d3d9_vb_original_result",201,0,0x1234,pack(frame,1)),
                ("model2_d3d9_original_present_result",frame,0,1,0)]
            self.write_log(p,rows)
            got=REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(got["original_gpu_surface_lineage_witnesses"],1)
            self.assertFalse(got["world_pass_proven"])
            for bad in ((18,7,0),(17,1000001,0),(17,7,1),(0,7,0)):
                mutated=rows.copy()
                mutated[3]=("model2_d3d9_source_gpu_surface_lineage",201,*bad)
                self.write_log(p,mutated)
                with self.assertRaisesRegex(ValueError,"GPU Surface"):
                    REPORT.audit_csv(p)
            self.write_log(p,rows[:4]+[rows[3]]+rows[4:])
            with self.assertRaisesRegex(ValueError,"GPU Surface"):
                REPORT.audit_csv(p)

    def test_original_gpu_surface_mutation_rejects_stale_vb_present(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"private_gpu_mutation.csv"
            rows=[
                ("model2_d3d9_vb_present_frame",201,7,1,0),
                ("model2_d3d9_vb_source",201,0x1234,pack(1,0x104),pack(4096,24)),
                ("model2_d3d9_source_draw_phase",201,7,12,pack(0x1234,1)),
                ("model2_d3d9_source_gpu_surface_lineage",201,7,3,0),
                ("model2_d3d9_vb_original_result",201,0,0x1234,pack(7,1)),
                ("model2_d3d9_original_present_result",7,0,1,0)]
            self.write_log(p,rows)
            baseline=REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(baseline["original_surface_failure_witnesses"],0)
            for method in range(1,5):
                rejected=("model2_d3d9_original_gpu_surface_mutation_rejected",
                          7,0x1234,12,method)
                self.write_log(p,rows[:-1]+[rejected,rows[-1]])
                result=REPORT.audit_csv(p)["sources"][0]
                self.assertEqual(result["original_surface_failure_witnesses"],1)
                self.assertEqual(result[
                    "distinct_scene_attested_physically_presented_material_present_frame_count"],0)
            for bad in ((0,0x1234,12,1),(7,0,12,1),
                        (7,0x1234,10000001,1),(7,0x1234,12,5)):
                rejected=("model2_d3d9_original_gpu_surface_mutation_rejected",*bad)
                self.write_log(p,rows[:-1]+[rejected,rows[-1]])
                with self.assertRaisesRegex(ValueError,"GPU Surface mutation"):
                    REPORT.audit_csv(p)

    def test_unmirrored_patch_and_process_vertices_revoke_original_vb(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/"unmirrored.csv"
            rows=[
                ("model2_d3d9_vb_present_frame",201,7,1,0),
                ("model2_d3d9_vb_source",201,0x1234,pack(1,0x104),pack(4096,24)),
                ("model2_d3d9_source_draw_phase",201,7,12,pack(0x1234,1)),
                ("model2_d3d9_vb_original_result",201,0,0x1234,pack(7,1)),
                ("model2_d3d9_original_present_result",7,0,1,0)]
            self.write_log(path,rows)
            good=REPORT.audit_csv(path)["sources"][0]
            self.assertEqual(good["original_surface_failure_witnesses"],0)
            # Frame 7 is neither frame 1 nor a multiple of 8/120; the
            # original failure still has to revoke its matching Draw.
            self.assertNotEqual(7%8,0)
            for method in range(1,6):
                shape=(7,0,pack(5,0),0) if method==5 else (
                    7,0x1234,pack(method,12 if method in (1,2) else 0),0)
                self.write_log(path,rows[:-1]+[
                    ("model2_d3d9_original_unmirrored_geometry",*shape),rows[-1]])
                rejected=REPORT.audit_csv(path)["sources"][0]
                self.assertEqual(rejected["original_surface_failure_witnesses"],1)
            for invalid in ((0,0,pack(5,0),0),
                            (7,0x1234,pack(5,0),0),
                            (7,0,pack(5,1),0),
                            (7,0,pack(5,0),1),
                            (7,0,pack(1,12),0),
                            (7,0x1234,pack(6,12),0),
                            (7,0x1234,pack(1,0),0),
                            (7,0x1234,pack(2,10000001),0)):
                self.write_log(path,rows[:-1]+[
                    ("model2_d3d9_original_unmirrored_geometry",*invalid),rows[-1]])
                with self.assertRaisesRegex(ValueError,"unmirrored geometry"):
                    REPORT.audit_csv(path)

    def test_raster_mode_lineage_and_unsupported_real_source(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/"raster.csv"
            rows=[
                ("model2_d3d9_vb_present_frame",201,7,1,0),
                ("model2_d3d9_vb_source",201,0x1234,pack(1,0x104),pack(4096,24)),
                ("model2_d3d9_source_draw_phase",201,7,12,pack(0x1234,1)),
                ("model2_d3d9_source_raster_mode_lineage",201,7,pack(1,2),3),
                ("model2_d3d9_vb_original_result",201,0,0x1234,pack(7,1)),
                ("model2_d3d9_original_present_result",7,0,1,0)]
            self.write_log(path,rows)
            good=REPORT.audit_csv(path)["sources"][0]
            self.assertEqual(good["original_raster_mode_lineage_witnesses"],1)
            self.assertEqual(good["original_surface_failure_witnesses"],0)
            self.write_log(path,rows[:-1]+[
                ("model2_d3d9_original_raster_mode_unsupported",7,2,0,0),rows[-1]])
            rejected=REPORT.audit_csv(path)["sources"][0]
            self.assertEqual(rejected["original_surface_failure_witnesses"],1)
            for bad in ((8,pack(1,2),3),(7,pack(1000001,2),3),
                        (7,pack(1,2),1000001),(0,pack(1,2),3)):
                mutated=list(rows)
                mutated[3]=("model2_d3d9_source_raster_mode_lineage",201,*bad)
                self.write_log(path,mutated)
                with self.assertRaisesRegex(ValueError,"raster-mode"):
                    REPORT.audit_csv(path)
            self.write_log(path,rows[:4]+[rows[3]]+rows[4:])
            with self.assertRaisesRegex(ValueError,"raster-mode"):
                REPORT.audit_csv(path)
            for invalid in ((0,1,0,0),(7,0,0,0),(7,4,0,0),(7,2,0,1)):
                self.write_log(path,rows[:-1]+[
                    ("model2_d3d9_original_raster_mode_unsupported",*invalid),rows[-1]])
                with self.assertRaisesRegex(ValueError,"raster mode"):
                    REPORT.audit_csv(path)
    def test_fixed_function_lineage_is_bound_to_exact_vb_present(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"fixed_function.csv"
            rows=[
                ("model2_d3d9_vb_present_frame",201,7,1,0),
                ("model2_d3d9_vb_source",201,0x1234,pack(1,0x104),pack(4096,24)),
                ("model2_d3d9_source_draw_phase",201,7,12,pack(0x1234,1)),
                ("model2_d3d9_source_fixed_function_lineage",201,7,pack(1,2),pack(3,4)),
                ("model2_d3d9_vb_original_result",201,0,0x1234,pack(7,1)),
                ("model2_d3d9_original_present_result",7,0,1,0)]
            self.write_log(p,rows)
            result=REPORT.audit_csv(p)
            self.assertEqual(result["sources"][0]["original_fixed_function_lineage_witnesses"],1)
            self.assertEqual(result["world_pass_proof"],"NOT_ESTABLISHED")
            for bad in ((8,pack(1,2),pack(3,4)),
                        (0,pack(1,2),pack(3,4)),
                        (7,pack(1_000_001,2),pack(3,4))):
                mutated=list(rows)
                mutated[3]=("model2_d3d9_source_fixed_function_lineage",201,*bad)
                self.write_log(p,mutated)
                with self.assertRaisesRegex(ValueError,"fixed-function"):
                    REPORT.audit_csv(p)
            self.write_log(p,rows[:4]+[rows[3]]+rows[4:])
            with self.assertRaisesRegex(ValueError,"fixed-function"):
                REPORT.audit_csv(p)

    def test_all_family_physical_draw_phase_is_witness_bound(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "physical_phase.csv"
            events = [
                ("model2_d3d9_vb_present_frame", 201, 7, 1, 0),
                ("model2_d3d9_vb_source", 201, 0x1234,
                 pack(1, 0x104), pack(4096, 24)),
                ("model2_d3d9_source_draw_phase", 201, 7, 12,
                 pack(0x1234, 1)),
                ("model2_d3d9_source_scene_phase", 201, 7, 1, 1),
                ("model2_d3d9_vb_original_result", 201, 0, 0x1234,
                 pack(7, 1)),
                ("model2_d3d9_original_present_result", 7, 0, 1, 0),
            ]
            self.write_log(p, events)
            src = REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(src["original_draw_phase_attested_witnesses"], 1)
            self.assertEqual(src["original_scene_phase_attested_witnesses"], 1)
            self.assertEqual(src["original_scene_ordinal_min"], 1)
            self.assertEqual(src["distinct_original_scene_ordinals"], 1)
            self.assertEqual(src["original_draw_phase_missing_witnesses"], 0)
            self.assertEqual(src["original_draw_ordinal_min"], 12)
            self.assertEqual(src["original_draw_ordinal_max"], 12)
            self.assertEqual(src["distinct_original_draw_ordinals"], 1)
            # Historic files still report physical Draw outcomes, but
            # cannot pass the new phase-attested source gate.
            self.write_log(p, [e for e in events
                               if e[0] != "model2_d3d9_source_draw_phase"])
            src = REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(src["original_draw_success_witnesses"], 1)
            self.assertEqual(src["original_draw_phase_missing_witnesses"], 1)
            self.assertEqual(src["distinct_original_draw_ordinals"], 0)
            for bad in ((7, 12, pack(0x4321, 1)),
                        (7, 12, pack(0x1234, 2)),
                        (8, 12, pack(0x1234, 1))):
                changed = list(events)
                changed[2] = ("model2_d3d9_source_draw_phase", 201, *bad)
                self.write_log(p, changed)
                src = REPORT.audit_csv(p)["sources"][0]
                self.assertEqual(
                    src["original_draw_phase_inconsistent_witnesses"], 1)
                self.assertEqual(
                    src["original_draw_result_inconsistent_witnesses"], 1)

    def test_unsampled_original_draw_failure_revokes_other_vb_candidate(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"draw_hresult.csv"
            events=[
                ("model2_d3d9_vb_present_frame", 501, 17, 1, 0),
                ("model2_d3d9_vb_source", 501, 0x1234,
                 pack(1, 0x104), pack(4096, 24)),
                ("model2_d3d9_source_draw_phase", 501,
                 17, 12, pack(0x1234, 1)),
                ("model2_d3d9_source_scene_phase", 501, 17, 1, 1),
                ("model2_d3d9_vb_original_result", 501,
                 0, 0x1234, pack(17, 1)),
                ("model2_d3d9_original_draw_failed", 17,
                 4, 0x8876086C, 0x4321),
                ("model2_d3d9_original_present_result", 17, 0, 1, 0),
            ]
            self.write_log(p,events)
            src=REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(src["original_draw_success_witnesses"],1)
            self.assertEqual(src["original_present_success_witnesses"],1)
            self.assertEqual(src["other_original_draw_failure_witnesses"],1)
            self.assertEqual(
                src["distinct_scene_attested_physically_presented_material_present_frame_count"],0)
            self.write_log(p,[e for e in events
                              if e[0]!="model2_d3d9_original_draw_failed"])
            src=REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(src["other_original_draw_failure_witnesses"],0)
            self.write_log(p,events[:-2]+[
                ("model2_d3d9_original_draw_failed", 17, 4, 0, 0x4321),
                events[-1]])
            with self.assertRaisesRegex(ValueError,"invalid original Draw failure"):
                REPORT.audit_csv(p)

    def test_original_scene_failure_invalidates_same_frame_vb_world_source(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"scene_hresult.csv"
            events=[
                ("model2_d3d9_vb_present_frame", 501, 17, 1, 0),
                ("model2_d3d9_vb_source", 501, 0x1234,
                 pack(1, 0x104), pack(4096, 24)),
                ("model2_d3d9_source_draw_phase", 501,
                 17, 12, pack(0x1234, 1)),
                ("model2_d3d9_source_scene_phase", 501, 17, 1, 1),
                ("model2_d3d9_vb_original_result", 501,
                 0, 0x1234, pack(17, 1)),
                ("model2_d3d9_original_scene_failed", 17,
                 2, 0x8876086C, 0x4321),
                ("model2_d3d9_original_present_result", 17, 0, 1, 0),
            ]
            self.write_log(p,events)
            report=REPORT.audit_csv(p)
            src=report["sources"][0]
            self.assertEqual(src["original_draw_success_witnesses"],1)
            self.assertEqual(src["original_present_success_witnesses"],1)
            self.assertEqual(
                src["original_scene_transition_failure_witnesses"],1)
            self.assertEqual(
                src["original_scene_phase_attested_witnesses"],1)
            self.assertEqual(
                src["distinct_scene_attested_physically_presented_material_present_frame_count"],
                0)
            self.write_log(p,[row for row in events if
                              row[0]!="model2_d3d9_original_scene_failed"])
            recovered=REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(
                recovered["original_scene_transition_failure_witnesses"],0)
            self.write_log(p,events[:-2]+[
                ("model2_d3d9_original_scene_failed", 17, 2, 0, 0x4321),
                events[-1]])
            with self.assertRaisesRegex(ValueError,
                                        "invalid original scene failure"):
                REPORT.audit_csv(p)

    def test_late_original_clear_failure_revokes_actual_vb_present(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"original_clear_hresult.csv"
            events=[
                ("model2_d3d9_vb_present_frame", 501, 17, 1, 0),
                ("model2_d3d9_vb_source", 501, 0x1234,
                 pack(1, 0x104), pack(4096, 24)),
                ("model2_d3d9_source_draw_phase", 501,
                 17, 12, pack(0x1234, 1)),
                ("model2_d3d9_source_scene_phase", 501, 17, 1, 1),
                ("model2_d3d9_vb_original_result", 501,
                 0, 0x1234, pack(17, 1)),
                # Original Draw was successful, but a subsequent real Clear
                # failed before this same physically successful Present.
                ("model2_d3d9_original_clear_failed", 17,
                 3, 0x8876086C, 0x4321),
                ("model2_d3d9_original_present_result", 17, 0, 1, 0),
            ]
            self.write_log(p,events)
            src=REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(src["original_clear_failure_witnesses"],1)
            self.assertEqual(src["original_draw_success_witnesses"],1)
            self.assertEqual(src["original_present_success_witnesses"],1)
            self.assertEqual(src[
                "distinct_scene_attested_physically_presented_material_present_frame_count"],0)
            self.write_log(p,[e for e in events if
                              e[0]!="model2_d3d9_original_clear_failed"])
            recovered=REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(recovered["original_clear_failure_witnesses"],0)
            for bad_result in (0,0x12345678):
                invalid=list(events)
                invalid[-2]=("model2_d3d9_original_clear_failed",17,
                             3,bad_result,0x4321)
                self.write_log(p,invalid)
                with self.assertRaisesRegex(ValueError,"invalid original Clear failure"):
                    REPORT.audit_csv(p)

    def test_real_palette_lineage_and_late_rejection(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"original_palette.csv"
            rows=[
                ("model2_d3d9_vb_present_frame",501,17,1,0),
                ("model2_d3d9_vb_source",501,0x1234,pack(1,0x104),pack(4096,24)),
                ("model2_d3d9_source_draw_phase",501,17,12,pack(0x1234,1)),
                ("model2_d3d9_source_palette_lineage",501,17,pack(2,3),0),
                ("model2_d3d9_vb_original_result",501,0,0x1234,pack(17,1)),
                ("model2_d3d9_original_present_result",17,0,1,0)]
            self.write_log(p,rows)
            good=REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(good["original_palette_lineage_witnesses"],1)
            self.assertEqual(good["original_surface_failure_witnesses"],0)
            reject=("model2_d3d9_original_palette_mutation_rejected",17,0x1234,12,2)
            self.write_log(p,rows[:-1]+[reject,rows[-1]])
            blocked=REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(blocked["original_surface_failure_witnesses"],1)
            for bad in ((18,pack(2,3),0),(17,pack(1000001,3),0),
                        (17,pack(2,3),1)):
                modified=list(rows)
                modified[3]=("model2_d3d9_source_palette_lineage",501,*bad)
                self.write_log(p,modified)
                with self.assertRaisesRegex(ValueError,"palette"):
                    REPORT.audit_csv(p)
            self.write_log(p,rows[:4]+[rows[3]]+rows[4:])
            with self.assertRaisesRegex(ValueError,"palette"):
                REPORT.audit_csv(p)
            for invalid in ((0,0x1234,12,1),(17,0,12,1),
                            (17,0x1234,10000001,1),(17,0x1234,12,3)):
                self.write_log(p,rows[:-1]+[
                    ("model2_d3d9_original_palette_mutation_rejected",*invalid),rows[-1]])
                with self.assertRaisesRegex(ValueError,"palette"):
                    REPORT.audit_csv(p)

    def test_real_texture_upload_lineage_and_late_stale_eye_rejection(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"original_texture_upload.csv"
            rows=[
                ("model2_d3d9_vb_present_frame",501,17,1,0),
                ("model2_d3d9_vb_source",501,0x1234,pack(1,0x104),pack(4096,24)),
                ("model2_d3d9_source_draw_phase",501,17,12,pack(0x1234,1)),
                ("model2_d3d9_source_scene_phase",501,17,1,1),
                ("model2_d3d9_source_texture_upload_lineage",501,17,3,0),
                ("model2_d3d9_vb_original_result",501,0,0x1234,pack(17,1)),
                ("model2_d3d9_original_present_result",17,0,1,0)]
            self.write_log(p,rows)
            good=REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(good["original_texture_upload_lineage_witnesses"],1)
            shape_rows=list(rows)
            shape_rows.insert(5,("model2_d3d9_source_texture_shape_lineage",
                                 501,17,pack(2,3),4))
            self.write_log(p,shape_rows)
            shape=REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(shape["original_texture_shape_lineage_witnesses"],1)
            for bad_shape in ((18,pack(2,3),4),
                               (17,pack(1_000_001,3),4),(17,pack(2,3),1_000_001)):
                altered=list(shape_rows)
                altered[5]=("model2_d3d9_source_texture_shape_lineage",501,*bad_shape)
                self.write_log(p,altered)
                with self.assertRaisesRegex(ValueError,"cube/volume/surface texture"):
                    REPORT.audit_csv(p)
            self.write_log(p,shape_rows[:6]+[shape_rows[5]]+shape_rows[6:])
            with self.assertRaisesRegex(ValueError,"cube/volume/surface texture"):
                REPORT.audit_csv(p)
            self.write_log(p,rows)
            locked=list(rows)
            locked[4]=("model2_d3d9_source_texture_upload_lineage",501,17,3,2)
            self.write_log(p,locked)
            self.assertEqual(REPORT.audit_csv(p)["sources"][0]["original_texture_upload_lineage_witnesses"],1)
            self.write_log(p,rows)
            self.assertEqual(good["original_surface_failure_witnesses"],0)
            rejection=("model2_d3d9_original_texture_upload_rejected",17,0x1234,12,0)
            self.write_log(p,rows[:-1]+[rejection,rows[-1]])
            blocked=REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(blocked["original_surface_failure_witnesses"],1)
            self.assertEqual(blocked[
                "distinct_scene_attested_physically_presented_material_present_frame_count"],0)
            for bad in ((0,0x1234,12,0),(17,0,12,0),
                        (17,0x1234,10000001,0),(17,0x1234,12,1)):
                self.write_log(p,rows[:-1]+[
                    ("model2_d3d9_original_texture_upload_rejected",*bad),rows[-1]])
                with self.assertRaisesRegex(ValueError,"texture upload"):
                    REPORT.audit_csv(p)
            for bad in ((18,3,0),(17,1000001,0),(17,1,1000001)):
                changed=list(rows)
                changed[4]=("model2_d3d9_source_texture_upload_lineage",501,*bad)
                self.write_log(p,changed)
                with self.assertRaisesRegex(ValueError,"texture upload"):
                    REPORT.audit_csv(p)
            self.write_log(p,rows[:5]+[rows[4]]+rows[5:])
            with self.assertRaisesRegex(ValueError,"texture upload"):
                REPORT.audit_csv(p)


    def test_original_dirty_mip_lineage_bound_to_same_physical_vb_draw(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"mip_source.csv"
            rows=[
                ("model2_d3d9_vb_present_frame",601,21,1,0),
                ("model2_d3d9_vb_source",601,0x1234,pack(1,0x104),pack(4096,24)),
                ("model2_d3d9_source_draw_phase",601,21,14,pack(0x1234,1)),
                ("model2_d3d9_source_scene_phase",601,21,1,1),
                ("model2_d3d9_source_texture_mip_lineage",601,21,pack(2,3),4),
                ("model2_d3d9_vb_original_result",601,0,0x1234,pack(21,1)),
                ("model2_d3d9_original_present_result",21,0,1,0),
            ]
            self.write_log(p,rows)
            result=REPORT.audit_csv(p)
            self.assertEqual(result["sources"][0]["original_texture_mip_lineage_witnesses"],1)
            self.assertEqual(result["world_pass_proof"],"NOT_ESTABLISHED")
            for bad in ((22,pack(2,3),4),(21,pack(1_000_001,3),4),
                        (21,pack(2,1_000_001),4),(21,pack(2,3),1_000_001)):
                changed=list(rows)
                changed[4]=("model2_d3d9_source_texture_mip_lineage",601,*bad)
                self.write_log(p,changed)
                with self.assertRaisesRegex(ValueError,"mip/dirty texture"):
                    REPORT.audit_csv(p)
            self.write_log(p,rows[:5]+[rows[4]]+rows[5:])
            with self.assertRaisesRegex(ValueError,"mip/dirty texture"):
                REPORT.audit_csv(p)
            for method in range(56,60):
                failure=("model2_d3d9_original_surface_failed",21,method,
                         0x8876086c,0x1234)
                self.write_log(p,rows[:-1]+[failure,rows[-1]])
                self.assertEqual(REPORT.audit_csv(p)["sources"][0]
                                 ["original_surface_failure_witnesses"],1)

    def test_original_texture_lod_revision_is_physical_and_bounded(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"lod.csv"
            rows=[
                ("model2_d3d9_vb_present_frame",801,27,1,0),
                ("model2_d3d9_vb_source",801,0x1234,pack(1,0x104),pack(4096,24)),
                ("model2_d3d9_source_draw_phase",801,27,9,pack(0x1234,1)),
                ("model2_d3d9_source_scene_phase",801,27,1,1),
                ("model2_d3d9_source_texture_lod_lineage",801,27,5,0),
                ("model2_d3d9_vb_original_result",801,0,0x1234,pack(27,1)),
                ("model2_d3d9_original_present_result",27,0,1,0),
            ]
            self.write_log(p,rows)
            good=REPORT.audit_csv(p)
            self.assertEqual(good["sources"][0]["original_texture_lod_lineage_witnesses"],1)
            self.assertEqual(good["world_pass_proof"],"NOT_ESTABLISHED")
            for bad in ((28,5,0),(27,1_000_001,0),(27,5,1)):
                changed=list(rows)
                changed[4]=("model2_d3d9_source_texture_lod_lineage",801,*bad)
                self.write_log(p,changed)
                with self.assertRaisesRegex(ValueError,"managed texture LOD"):
                    REPORT.audit_csv(p)
            self.write_log(p,rows[:5]+[rows[4]]+rows[5:])
            with self.assertRaisesRegex(ValueError,"managed texture LOD"):
                REPORT.audit_csv(p)

    def test_real_original_surface_failure_rejects_same_present_material(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"original_surface_hresult.csv"
            prefix=[
                ("model2_d3d9_vb_present_frame", 501, 17, 1, 0),
                ("model2_d3d9_vb_source", 501, 0x1234,
                 pack(1, 0x104), pack(4096, 24)),
                ("model2_d3d9_source_draw_phase", 501,
                 17, 12, pack(0x1234, 1)),
                ("model2_d3d9_source_scene_phase", 501, 17, 1, 1),
                ("model2_d3d9_vb_original_result", 501,
                 0, 0x1234, pack(17, 1)),
            ]
            original_present=("model2_d3d9_original_present_result", 17, 0, 1, 0)
            for method in range(1, 71):
                failure=("model2_d3d9_original_surface_failed",17,
                         method,0x8876086C,0x4321)
                self.write_log(p,prefix+[failure,original_present])
                source=REPORT.audit_csv(p)["sources"][0]
                self.assertEqual(source["original_surface_failure_witnesses"],1)
                self.assertEqual(
                    source["distinct_scene_attested_physically_presented_material_present_frame_count"],0)
                self.write_log(p,prefix+[original_present])
                self.assertEqual(REPORT.audit_csv(p)["sources"][0]
                                 ["original_surface_failure_witnesses"],0)
            for family in (1,2):
                event=("model2_d3d9_original_buffer_mutation_rejected",
                       17,0x1234,12,family)
                self.write_log(p,prefix+[event,original_present])
                got=REPORT.audit_csv(p)["sources"][0]
                self.assertEqual(got["original_surface_failure_witnesses"],1)
                for bad in ((17,0,12,family),(17,0x1234,10000001,family),
                            (17,0x1234,12,3)):
                    self.write_log(p,prefix+[(
                        "model2_d3d9_original_buffer_mutation_rejected",*bad),
                        original_present])
                    with self.assertRaisesRegex(ValueError,"invalid original buffer mutation"):
                        REPORT.audit_csv(p)
            unobserved=("model2_d3d9_original_buffer_hook_unobserved",
                        17,0,0,0)
            self.write_log(p,prefix+[unobserved,original_present])
            self.assertEqual(REPORT.audit_csv(p)["sources"][0]
                             ["original_surface_failure_witnesses"],1)
            for bad in ((17,0,1,0),(17,123,0,0),(17,0,0,1)):
                self.write_log(p,prefix+[(
                    "model2_d3d9_original_buffer_hook_unobserved",*bad),
                    original_present])
                with self.assertRaisesRegex(ValueError,"invalid original buffer hook coverage"):
                    REPORT.audit_csv(p)
            drift=("model2_d3d9_original_lineage_drift",17,1,0x1234,12)
            self.write_log(p,prefix+[drift,original_present])
            source=REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(source["original_surface_failure_witnesses"],1)
            self.assertEqual(
                source["distinct_scene_attested_physically_presented_material_present_frame_count"],0)
            for bad in ((5,0x1234,12),(1,0,12),(1,0x1234,0)):
                self.write_log(p,prefix+[(
                    "model2_d3d9_original_lineage_drift",17,*bad),original_present])
                with self.assertRaisesRegex(ValueError,"invalid original surface failure"):
                    REPORT.audit_csv(p)
            for active_mask in (1,2,3,0x80000000):
                active=("model2_d3d9_original_instancing_active",17,
                        active_mask,0,0)
                self.write_log(p,prefix+[active,original_present])
                sample=REPORT.audit_csv(p)["sources"][0]
                self.assertEqual(sample["original_surface_failure_witnesses"],1)
                self.assertEqual(sample[
                    "distinct_scene_attested_physically_presented_material_present_frame_count"],0)
            for bad in ((17,0,0,0),(0,1,0,0),
                        (17,1,1,0),(17,1,0,1)):
                self.write_log(p,prefix+[(
                    "model2_d3d9_original_instancing_active",*bad),original_present])
                with self.assertRaisesRegex(ValueError,"invalid original active instancing source"):
                    REPORT.audit_csv(p)
            for frequency in (0,2,0x40000001,0x80000001):
                inst=("model2_d3d9_original_instancing_unsupported",17,
                      0,0x1234,frequency)
                self.write_log(p,prefix+[inst,original_present])
                source=REPORT.audit_csv(p)["sources"][0]
                self.assertEqual(source["original_surface_failure_witnesses"],1)
                self.assertEqual(source[
                    "distinct_scene_attested_physically_presented_material_present_frame_count"],0)
            for bad in ((17,0,0x1234,1),(17,0,0,2),
                        (0,0,0x1234,2),(17,0,0x1234,0x100000000)):
                self.write_log(p,prefix+[(
                    "model2_d3d9_original_instancing_unsupported",*bad),
                    original_present])
                with self.assertRaisesRegex(ValueError,"invalid original instancing source"):
                    REPORT.audit_csv(p)
            for stage in (1, 2):
                apply=("model2_d3d9_original_stateblock_mutation",17,
                       stage,0x1234,0)
                self.write_log(p,prefix+[apply,original_present])
                blocked=REPORT.audit_csv(p)["sources"][0]
                self.assertEqual(blocked["original_surface_failure_witnesses"],1)
                self.assertEqual(blocked[
                    "distinct_scene_attested_physically_presented_material_present_frame_count"],0)
            for bad in ((0,1,0x1234,0),(17,3,0x1234,0),
                        (17,1,0,0),(17,1,0x1234,0x100000000)):
                event=("model2_d3d9_original_stateblock_mutation",*bad)
                self.write_log(p,prefix+[event,original_present])
                with self.assertRaisesRegex(ValueError,"invalid original stateblock mutation"):
                    REPORT.audit_csv(p)
            for bad in ((0,0x8876086C,0x4321),
                        (71,0x8876086C,0x4321),
                        (1,0,0x4321),
                        (2,0x8876086C,0)):
                failure=("model2_d3d9_original_surface_failed",17,*bad)
                self.write_log(p,prefix+[failure,original_present])
                with self.assertRaisesRegex(ValueError,"invalid original surface failure"):
                    REPORT.audit_csv(p)

    def test_unknown_failed_or_outside_original_scene_cannot_attest_world(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "scene_phase.csv"
            base = [
                ("model2_d3d9_vb_present_frame", 301, 7, 1, 0),
                ("model2_d3d9_vb_source", 301, 0x1234,
                 pack(1, 0x104), pack(4096, 24)),
                ("model2_d3d9_source_draw_phase", 301, 7, 18,
                 pack(0x1234, 1)),
                ("model2_d3d9_vb_original_result", 301, 0, 0x1234,
                 pack(7, 1)),
                ("model2_d3d9_original_present_result", 7, 0, 1, 0),
            ]
            self.write_log(p, base)
            src = REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(src["original_scene_phase_missing_witnesses"], 1)
            self.assertIsNone(src["original_scene_ordinal_min"])
            self.write_log(p, base[:3] + [
                ("model2_d3d9_source_scene_phase", 301, 7, 1, 0)
            ] + base[3:])
            src = REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(src["original_scene_phase_inactive_witnesses"], 1)
            self.assertEqual(src["original_scene_phase_attested_witnesses"], 0)
            self.write_log(p, base[:3] + [
                ("model2_d3d9_source_scene_phase", 301, 8, 1, 1)
            ] + base[3:])
            src = REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(
                src["original_scene_phase_inconsistent_witnesses"], 1)
            self.write_log(p, base[:3] + [
                ("model2_d3d9_source_scene_phase", 301, 7, 0, 1)
            ] + base[3:])
            with self.assertRaisesRegex(ValueError, "invalid original scene phase"):
                REPORT.audit_csv(p)

    def test_up_and_vb_cannot_reuse_same_original_draw_ordinal(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "all_family_order.csv"
            self.write_log(p, [
                ("model2_d3d9_source_draw_phase", 3, 7, 12, pack(0x4321, 3)),
                ("model2_d3d9_vb_source", 4, 0x1234,
                 pack(1, 0x104), pack(4096, 24)),
                ("model2_d3d9_source_draw_phase", 4, 7, 12, pack(0x1234, 1)),
            ])
            with self.assertRaisesRegex(
                    ValueError, "duplicate all-family original Draw phase"):
                REPORT.audit_csv(p)

    def test_original_com_draw_result_binds_to_exact_source_witness(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "results.csv"
            self.write_log(path, [
                ("model2_d3d9_vb_present_frame", 201, 7, 1, 0),
                ("model2_d3d9_vb_source", 201, 0x1234,
                 pack(1, 0x104), pack(4096, 24)),
                ("model2_d3d9_vb_original_result", 201, 0, 0x1234, pack(7, 1)),
                ("model2_d3d9_vb_present_frame", 202, 8, 1, 0),
                ("model2_d3d9_vb_source", 202, 0x1234,
                 pack(1, 0x104), pack(4096, 24)),
                ("model2_d3d9_vb_original_result", 202, 0x8876086c,
                 0x1234, pack(8, 1)),
                ("model2_d3d9_vb_present_frame", 203, 9, 1, 0),
                ("model2_d3d9_vb_source", 203, 0x1234,
                 pack(1, 0x104), pack(4096, 24)),
                ("model2_d3d9_vb_original_result", 203, 0, 0x9999, pack(9, 1)),
                ("model2_d3d9_vb_present_frame", 204, 10, 1, 0),
                ("model2_d3d9_vb_source", 204, 0x1234,
                 pack(1, 0x104), pack(4096, 24)),
            ])
            src = REPORT.audit_csv(path)["sources"][0]
            self.assertEqual(src["original_draw_success_witnesses"], 1)
            self.assertEqual(src["original_draw_failure_witnesses"], 1)
            self.assertEqual(src["original_draw_result_inconsistent_witnesses"], 1)
            self.assertEqual(src["original_draw_result_missing_witnesses"], 1)
            self.assertEqual(
                src["distinct_successful_projected_material_present_frame_count"], 0)
            self.write_log(path, [
                ("model2_d3d9_vb_source", 201, 0x1234,
                 pack(1, 0x104), pack(4096, 24)),
                ("model2_d3d9_vb_original_result", 201, 0, 0x1234, pack(7, 1)),
                ("model2_d3d9_vb_original_result", 201, 0, 0x1234, pack(7, 1)),
            ])
            with self.assertRaisesRegex(ValueError, "duplicate original Draw result"):
                REPORT.audit_csv(path)

    def test_physical_original_present_must_succeed_after_draw(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "physical_present.csv"
            events = []
            for witness, frame in ((21, 7), (22, 8), (23, 9)):
                events.extend([
                    ("model2_d3d9_vb_present_frame", witness, frame, 1, 0),
                    ("model2_d3d9_vb_source", witness, 0x1234,
                     pack(1, 0x104), pack(4096, 24)),
                    ("model2_d3d9_vb_original_result", witness, 0, 0x1234,
                     pack(frame, 1)),
                ])
            events.extend([
                ("model2_d3d9_original_present_result", 7, 0, 1, 0),
                ("model2_d3d9_original_present_result", 8, 0x88760868, 1, 0),
            ])
            self.write_log(path, events)
            src = REPORT.audit_csv(path)["sources"][0]
            self.assertEqual(src["original_draw_success_witnesses"], 3)
            self.assertEqual(src["original_present_success_witnesses"], 1)
            self.assertEqual(src["original_present_failed_witnesses"], 1)
            self.assertEqual(src["original_present_missing_witnesses"], 1)
            self.write_log(path, events + [
                ("model2_d3d9_original_present_result", 7, 0, 1, 0),
            ])
            with self.assertRaisesRegex(ValueError, "duplicate physical Present"):
                REPORT.audit_csv(path)

    def test_join_indexed_stream_camera_and_texture_without_world_proof(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "probe.csv"
            self.write_log(p, [
                ("model2_d3d9_draw_origin", 3, 2, 0x1234, pack(2, 4)),
                ("model2_d3d9_vb_present_frame", 101, 17, 2, 0),
                ("model2_d3d9_vb_source", 101, 0x1234,
                 pack(2, 0x112), pack(65536, 20)),
                ("model2_d3d9_vb_stream", 101, pack(8, 0), 0, 12),
                ("model2_d3d9_vb_draw_args", 101, 0xFFFFFFFE,
                 pack(4, 5), pack(15, 2)),
                ("model2_d3d9_ib_source", 101, 101, 1200, 0),
                ("model2_d3d9_vb_camera", 101, 1, 2, 3),
                ("model2_d3d9_vb_texture", 101, 21, 64, 128),
                ("model2_d3d9_vb_sampler", 101,
                 pack(1, 3), pack(2, 2), pack(0, 0)),
                ("model2_d3d9_vb_alpha", 101,
                 pack(1, 7), 128, 0),
                ("model2_d3d9_vb_raster", 101,
                 pack(3, 4), pack(1, 0), pack(0, 0)),
                ("model2_d3d9_source_depth_cull", 101,
                 pack(1, 4), pack(1, 3), 17),
                ("model2_d3d9_source_vertex_pipeline",101,
                 pack(2,1),pack(0,0),pack(0,17)),
                ("model2_d3d9_source_fill",101,3,17,pack(0x112,20)),
                ("model2_d3d9_vb_write_color", 101,
                 pack(0, 15), 0, 0),
                ("model2_d3d9_source_color_mask", 101,
                 15, pack(0x112, 20), 17),
                ("model2_d3d9_source_blend", 101,
                 pack(1, 9), pack(1, 1), 0),
                ("model2_d3d9_source_alpha_compare", 101,
                 pack(1, 7), 128, pack(0x112, 20)),
                ("model2_d3d9_source_sampler_mip", 101,
                 0, 1, 17),
                ("model2_d3d9_source_stage0_color", 101,
                 pack(4, 2), 0, pack(0x112, 20)),
                ("model2_d3d9_source_stage0_alpha", 101,
                 pack(4, 2), 0, pack(0x112, 20)),
                ("model2_d3d9_source_stage1_specular", 101,
                 1, 1, pack(0, 0)),
                ("model2_d3d9_source_stage_result", 101,
                 1, 1, pack(0x112, 20)),
                ("model2_d3d9_cpu_vb_join_batch", 101,
                 pack(2, 2), pack(0, 0), 0x4567),
                ("model2_d3d9_cpu_vb_matrix_identity", 101,
                 0x1000, 0, 0x4567),
                ("model2_d3d9_cpu_vb_matrix_content", 101,
                 0xAABBCCDD, 0, 0x1000),
                ("model2_d3d9_cpu_vb_geometry_command", 101,
                 0x77, 0, 0xAABBCCDD),
                ("model2_d3d9_source_pass_context", 101,
                 17, 0xEFD, pack(640, 480)),
                ("model2_d3d9_vb_present_frame", 102, 17, 2, 0),
                ("model2_d3d9_vb_source", 102, 0x1234,
                 pack(2, 0x112), pack(65536, 20)),
                ("model2_d3d9_vb_stream", 102, pack(8, 8), 0, 12),
                ("model2_d3d9_vb_texture", 102, 21, 64, 128),
                ("model2_d3d9_vb_camera_unknown", 102, 0, 0, 0),
                ("model2_d3d9_source_color_mask", 102,
                 0, pack(0x112, 20), 17),
                ("model2_d3d9_source_vertex_pipeline",102,
                 pack(2,1),pack(0,0),pack(0,18)),
                ("model2_d3d9_source_fill",102,2,17,pack(0x112,20)),
                ("model2_d3d9_ib_source", 99, 101, 500, 0),
            ])
            result = REPORT.audit_csv(p)
            self.assertEqual(result["caller_count"], 1)
            self.assertEqual(result["witness_count"], 3)
            self.assertEqual(result["orphan_metadata_count"], 1)
            self.assertEqual(result["world_pass_proof"], "NOT_ESTABLISHED")
            src = result["sources"][0]
            self.assertEqual(src["caller_rva_hex"], "0x00001234")
            self.assertEqual(src["draw_family"], "DrawIndexedPrimitive")
            self.assertEqual(src["observations"], 2)
            self.assertEqual(src["vb_byte_sizes"], [65536])
            self.assertEqual(src["strides"], [20])
            self.assertEqual(src["index_formats"], [101])
            self.assertEqual(src["texture_sizes"], [[64, 128]])
            self.assertEqual(src["sampler_modes"], [[1, 3, 2, 2, 0, 0]])
            self.assertEqual(src["alpha_test_modes"], [[1, 7, 128]])
            self.assertEqual(src["raster_modes"], [[3, 4, 1, 0, 0, 0]])
            self.assertEqual(src["original_source_depth_cull_status"],
                             ["supported"])
            self.assertEqual(src["original_source_vertex_pipeline_status"],
                             ["present_mismatch","supported"])
            self.assertEqual(src["original_source_fill_status"],
                             ["solid","unsupported"])
            self.assertEqual(src["color_write_modes"], [[0, 15]])
            self.assertEqual(src["original_source_color_mask_modes"], [0, 15])
            self.assertEqual(src["original_depth_only_draw_witnesses"], 1)
            self.assertEqual(src["original_source_blend_modes"],
                             ["multiply_color"])
            self.assertEqual(src["original_source_alpha_compare_modes"],
                             ["greater_equal"])
            self.assertEqual(src["original_source_alpha_incompatible_witnesses"],
                             0)
            self.assertEqual(src["original_source_mip_status"],
                             ["mip_disabled"])
            self.assertEqual(
                src["original_source_blend_unsupported_witnesses"], 0)
            self.assertEqual(src["original_source_color_mask_mismatch_witnesses"], 0)
            self.assertEqual(src["original_material_complete_witnesses"], 1)
            self.assertEqual(src["original_material_current_result_witnesses"], 1)
            self.assertEqual(src["original_material_mismatched_witnesses"], 0)
            self.assertEqual(src["original_material_stage_states"],
                             [[4, 2, 0, 4, 2, 0, 1, 1, 0, 0]])
            self.assertEqual(
                src["distinct_original_material_present_frame_count"], 1)
            self.assertTrue(src["has_camera_matrix"])
            self.assertTrue(src["writeonly_vb_seen"])
            self.assertEqual(src["cpu_projector_correlated_batches"], 1)
            self.assertEqual(src["cpu_projector_source_callers"], ["0x00004567"])
            self.assertEqual(src["cpu_projector_complete_triangles"], 2)
            self.assertEqual(src["distinct_present_frame_count"], 1)
            self.assertEqual(src["distinct_correlated_present_frame_count"], 1)
            self.assertEqual(src["cpu_matrix_consistent_candidate_batches"], 1)
            self.assertEqual(src["cpu_matrix_content_fingerprints"],
                             ["0xaabbccdd"])
            self.assertEqual(
                src["distinct_matrix_consistent_present_frame_count"], 1)
            self.assertEqual(src["cpu_geometry_command_candidate_batches"], 1)
            self.assertEqual(src["cpu_geometry_command_words"], ["0x00000077"])
            self.assertEqual(
                src["distinct_geometry_command_present_frame_count"], 1)
            self.assertEqual(src["distinct_context_present_frame_count"], 1)
            self.assertEqual(src["original_main_full_view_depth_candidates"], 1)
            self.assertEqual(src["same_witness_joint_candidates"], 1)
            self.assertEqual(
                src["distinct_same_witness_joint_present_frame_count"], 1)
            self.assertEqual(src["same_witness_joint_cpu_matrix_commands"], [
                ["0x00004567", "0x00001000", "0xaabbccdd", "0x00000077"]])
            self.assertEqual(src["source_contexts"], [[1, 640, 480, 3837]])
            self.assertFalse(src["world_pass_proven"])

    def test_original_source_alpha_join_rejects_fvf_and_independent_vb(self):
        alpha=REPORT.original_d3d9_alpha_compare_status
        expected=(pack(1,6),127,pack(0x144,28))
        self.assertEqual(alpha(expected,0x144,28),"not_equal")
        self.assertEqual(alpha(expected,0x144,24),"fvf_stride_mismatch")
        self.assertEqual(alpha(expected,0x146,28),"fvf_stride_mismatch")
        self.assertEqual(alpha(expected,0x144,28,
                               (pack(1,3),127,0)),"vb_alpha_mismatch")
        self.assertEqual(alpha(expected,0x144,28,
                               (pack(1,6),127,0)),"not_equal")
        self.assertEqual(alpha((pack(0,99),127,pack(0x144,28)),
                               0x144,28),"disabled")
        self.assertEqual(alpha((pack(1,9),127,pack(0x144,28)),
                               0x144,28),"unsupported")

    def test_duplicate_original_alpha_source_witness_conflict_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"duplicate-alpha.csv"
            self.write_log(p,[
                ("model2_d3d9_source_alpha_compare",19,
                 pack(1,2),127,pack(0x144,28)),
                ("model2_d3d9_source_alpha_compare",19,
                 pack(1,3),127,pack(0x144,28)),
            ])
            with self.assertRaisesRegex(
                    ValueError,"conflicting original alpha witness"):
                REPORT.audit_csv(p)

    def test_source_separate_alpha_same_draw_fvf_and_rgb_mode(self):
        state=REPORT.original_d3d9_separate_alpha_status
        blend=REPORT.original_d3d9_blend_mode
        rgb=(pack(1,5),pack(6,1),1)
        separate=(pack(2,1),1,pack(0x144,28))
        self.assertEqual(state(rgb,separate,0x144,28),"supported")
        self.assertEqual(blend(rgb,separate,0x144,28),
                         "source_alpha_over_separate_alpha")
        self.assertEqual(state(rgb,None,0x144,28),"missing_source")
        self.assertEqual(blend(rgb,None,0x144,28),"unsupported")
        self.assertEqual(state(rgb,separate,0x144,24),"fvf_stride_mismatch")
        self.assertEqual(blend(rgb,separate,0x144,24),"unsupported")
        self.assertEqual(state(rgb,(pack(11,1),1,pack(0x144,28)),
                               0x144,28),"unsupported")
        self.assertEqual(state(rgb,(pack(2,1),2,pack(0x144,28)),
                               0x144,28),"unsupported")
        self.assertEqual(state((pack(1,5),pack(6,1),0),
                               separate,0x144,28),"unexpected_alpha")
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"separate.csv"
            self.write_log(p,[
                ("model2_d3d9_vb_source",27,0x1234,
                 pack(1,0x144),pack(2048,28)),
                ("model2_d3d9_vb_present_frame",27,17,1,0),
                ("model2_d3d9_source_blend",27,*rgb),
                ("model2_d3d9_source_separate_alpha_blend",27,*separate),
            ])
            report=REPORT.audit_csv(p)
            src=report["sources"][0]
            self.assertEqual(src["original_source_blend_modes"],
                             ["source_alpha_over_separate_alpha"])
            self.assertEqual(src["original_source_separate_alpha_status"],
                             ["supported"])
            self.assertEqual(src["original_source_separate_alpha_invalid_witnesses"],0)
            self.assertFalse(src["world_pass_proven"])
            # Same GPU Draw ID with another FVF is never a valid source.
            separate_bad=(pack(2,1),1,pack(0x144,24))
            self.write_log(p,[
                ("model2_d3d9_vb_source",27,0x1234,
                 pack(1,0x144),pack(2048,28)),
                ("model2_d3d9_vb_present_frame",27,17,1,0),
                ("model2_d3d9_source_blend",27,*rgb),
                ("model2_d3d9_source_separate_alpha_blend",27,*separate_bad),
            ])
            broken=REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(
                broken["original_source_separate_alpha_status"],
                ["fvf_stride_mismatch"])
            self.assertEqual(
                broken["original_source_blend_unsupported_witnesses"],1)

    def test_original_alpha_write_mask_makes_split_factors_inert(self):
        status=REPORT.original_d3d9_separate_alpha_status
        blend=REPORT.original_d3d9_blend_mode
        rgb=(pack(1,5),pack(6,1),1)
        full=(7,pack(0x144,28),17) # actual same Draw/Present alpha off
        self.assertEqual(status(
            rgb,None,0x144,28,full,17),"masked_inert")
        self.assertEqual(blend(
            rgb,None,0x144,28,full,17),"source_alpha_over")
        self.assertEqual(status(
            rgb,None,0x144,28,full,18),"missing_source")
        self.assertEqual(status(
            rgb,None,0x144,28,(15,pack(0x144,28),17),17),
            "missing_source")
        self.assertEqual(status(
            rgb,None,0x144,28,(7,pack(0x144,24),17),17),
            "missing_source")
        # A separately logged source cannot have a foreign declaration,
        # even if a same-Present color mask would make it inert.
        wrong=(pack(2,1),1,pack(0x144,24))
        self.assertEqual(status(
            rgb,wrong,0x144,28,full,17),"fvf_stride_mismatch")
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"masked-split.csv"
            self.write_log(p,[
                ("model2_d3d9_vb_source",77,0x55,
                 pack(1,0x144),pack(2048,28)),
                ("model2_d3d9_vb_present_frame",77,17,1,0),
                ("model2_d3d9_source_blend",77,*rgb),
                ("model2_d3d9_source_color_mask",77,
                 7,pack(0x144,28),17),
            ])
            src=REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(src["original_source_separate_alpha_status"],
                             ["masked_inert"])
            self.assertEqual(src["original_source_blend_modes"],
                             ["source_alpha_over"])
            self.assertFalse(src["world_pass_proven"])

    def test_conflicting_separate_alpha_witness_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"conflict.csv"
            self.write_log(p,[
                ("model2_d3d9_source_separate_alpha_blend",3,
                 pack(2,1),1,pack(0x144,28)),
                ("model2_d3d9_source_separate_alpha_blend",3,
                 pack(5,6),1,pack(0x144,28)),
            ])
            with self.assertRaisesRegex(
                    ValueError,"conflicting separate-alpha Draw witness"):
                REPORT.audit_csv(p)

    def test_in_place_matrix_drift_is_not_authentic_world_evidence(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "matrix.csv"
            self.write_log(p, [
                ("model2_d3d9_vb_present_frame", 77, 8, 1, 0),
                ("model2_d3d9_vb_source", 77, 0x111,
                 pack(1, 0x112), pack(2048, 20)),
                ("model2_d3d9_cpu_vb_join_batch", 77,
                 pack(2, 2), 0, 0x55),
                ("model2_d3d9_cpu_vb_matrix_identity", 77,
                 0x1000, 0, 0x55),
                ("model2_d3d9_cpu_vb_matrix_content", 77,
                 0xABC123, 1, 0x1000),
                ("model2_d3d9_cpu_vb_geometry_command", 77,
                 0x77, 1, 0xABC123),
                ("model2_d3d9_source_pass_context", 77,
                 9, 0x2FE, pack(256, 256)),
            ])
            item = REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(
                item["cpu_matrix_mixed_or_inconsistent_batches"], 1)
            self.assertEqual(
                item["cpu_matrix_consistent_candidate_batches"], 0)
            self.assertEqual(
                item["distinct_matrix_consistent_present_frame_count"], 0)
            self.assertEqual(item["cpu_geometry_command_mixed_batches"], 1)
            self.assertEqual(
                item["distinct_geometry_command_present_frame_count"], 0)
            self.assertEqual(item["source_context_frame_mismatches"], 1)
            self.assertEqual(item["original_main_full_view_depth_candidates"], 0)
            self.assertEqual(item["same_witness_joint_candidates"], 0)
            self.assertFalse(item["world_pass_proven"])


    def test_original_main_scissor_bounds_fail_closed(self):
        # Existing no-scissor evidence remains compatible. New bit 13
        # requires scissor-read success and exact source dimensions.
        self.assertTrue(REPORT.original_main_depth_view_candidate(0xEFD))
        self.assertTrue(REPORT.original_main_depth_view_candidate(0x3FFD))
        self.assertFalse(REPORT.original_main_depth_view_candidate(0xFFD))
        self.assertFalse(REPORT.original_main_depth_view_candidate(0x1FFD))
        self.assertFalse(REPORT.original_main_depth_view_candidate(0x2FFD))
        self.assertFalse(REPORT.original_main_depth_view_candidate(0x3EFD))
        self.assertFalse(REPORT.original_main_depth_view_candidate(0x3FFE))
        self.assertFalse(REPORT.original_main_depth_view_candidate(0x4EFD))
        self.assertFalse(REPORT.original_main_depth_view_candidate(0x2FD))
        # Match original CPU matrix/command and Present on the SAME
        # witness; never infer world ownership from the source scissor.
        data={
            "source_context": (17, 0x3FFD, pack(640, 480)),
            "cpu_join": (pack(2, 2), 0, 0x55),
            "matrix_identity": (0x1000, 0, 0x55),
            "matrix_content": (0xAABB, 0, 0x1000),
            "geometry_command": (0x77, 0, 0xAABB),
        }
        self.assertEqual(
            REPORT.same_witness_projection_pass_candidate(data, 17, 0x1234),
            (0x55, 0x1000, 0xAABB, 0x77))
        data["source_context"]=(17, 0x1FFD, pack(640, 480))
        self.assertIsNone(REPORT.same_witness_projection_pass_candidate(
            data, 17, 0x1234))
        data["source_context"]=(17, 0x3FFD, pack(640, 480))
        self.assertIsNone(REPORT.same_witness_projection_pass_candidate(
            data, 18, 0x1234))
        # A genuine same-Present source FILLMODE is NOT sufficient if it
        # belongs to another FVF or another Draw/vertex declaration.
        data["origin"]=(0x1234,pack(1,0x144),pack(2048,28))
        data["source_fill"]=(3,17,pack(0x144,28))
        self.assertEqual(
            REPORT.same_witness_projection_pass_candidate(data,17,0x1234),
            (0x55,0x1000,0xAABB,0x77))
        data["source_fill"]=(3,17,pack(0x144,24))
        self.assertIsNone(REPORT.same_witness_projection_pass_candidate(
            data,17,0x1234))
        data["source_fill"]=(2,17,pack(0x144,28))
        self.assertIsNone(REPORT.same_witness_projection_pass_candidate(
            data,17,0x1234))
        data["source_fill"]=(3,17,pack(0x144,28))
        data["source_vertex_pipeline"]=(pack(2,1),pack(0,0),pack(0,17))
        self.assertEqual(
            REPORT.same_witness_projection_pass_candidate(data,17,0x1234),
            (0x55,0x1000,0xAABB,0x77))
        data["source_vertex_pipeline"]=(pack(2,0),pack(0,0),pack(0,17))
        self.assertIsNone(REPORT.same_witness_projection_pass_candidate(
            data,17,0x1234))

    def test_disjoint_draw_witnesses_do_not_join(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "disjoint.csv"
            events = [
                ("model2_d3d9_vb_present_frame", 201, 70, 1, 0),
                ("model2_d3d9_vb_source", 201, 0x1234,
                 pack(1, 0x112), pack(4096, 20)),
                ("model2_d3d9_cpu_vb_join_batch", 201,
                 pack(2, 2), 0, 0x55),
                ("model2_d3d9_cpu_vb_matrix_identity", 201, 0x1000, 0, 0x55),
                ("model2_d3d9_cpu_vb_matrix_content", 201, 0xaabb, 0, 0x1000),
                ("model2_d3d9_cpu_vb_geometry_command", 201,
                 0x77, 0, 0xaabb),
                ("model2_d3d9_vb_present_frame", 202, 70, 1, 0),
                ("model2_d3d9_vb_source", 202, 0x1234,
                 pack(1, 0x112), pack(4096, 20)),
                ("model2_d3d9_source_pass_context", 202,
                 70, 0xEFD, pack(640, 480)),
            ]
            self.write_log(p, events)
            src = REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(src["distinct_matrix_consistent_present_frame_count"], 1)
            self.assertEqual(src["distinct_context_present_frame_count"], 1)
            self.assertEqual(src["same_witness_joint_candidates"], 0)
            self.assertFalse(src["world_pass_proven"])
            # Matching one ID but not one original Present still fails.
            bad = events[:6] + [("model2_d3d9_source_pass_context", 201,
                                 71, 0xEFD, pack(640, 480))]
            self.write_log(p, bad)
            self.assertEqual(REPORT.audit_csv(p)["sources"][0][
                "same_witness_joint_candidates"], 0)
            bad[-1] = ("model2_d3d9_source_pass_context", 201,
                       70, 0x2FD, pack(640, 480))
            self.write_log(p, bad)
            self.assertEqual(REPORT.audit_csv(p)["sources"][0][
                "same_witness_joint_candidates"], 0)

    def test_original_fill_mode_is_same_draw_original_fvf_and_present(self):
        status=REPORT.original_d3d9_fill_status
        tag=pack(0x144,28)
        self.assertEqual(status(None,17,0x144,28),"unobserved")
        self.assertEqual(status((3,17,tag),17,0x144,28),"solid")
        self.assertEqual(status((3,18,tag),17,0x144,28),"present_mismatch")
        self.assertEqual(status((3,17,tag),17,0x144,24),"source_layout_mismatch")
        self.assertEqual(status((2,17,tag),17,0x144,28),"unsupported")
        self.assertEqual(status((1,17,tag),17,0x144,28),"unsupported")
        self.assertEqual(status((999,17,tag),17,0x144,28),"unsupported")

    def test_original_vertex_pipeline_same_draw_and_present_only(self):
        status=REPORT.original_d3d9_vertex_pipeline_status
        good=(pack(2,1),pack(0,0),pack(0,17))
        self.assertEqual(status(None,17),"unobserved")
        self.assertEqual(status(good,17),"supported")
        self.assertEqual(status(good,18),"present_mismatch")
        self.assertEqual(status((pack(1,1),pack(0,0),pack(0,17)),17),
                         "unsupported")  # FLAT shading
        self.assertEqual(status((pack(2,0),pack(0,0),pack(0,17)),17),
                         "unsupported")  # original CLIPPING disabled
        self.assertEqual(status((pack(2,1),pack(1,0),pack(0,17)),17),
                         "unsupported")  # user clip plane
        self.assertEqual(status((pack(2,1),pack(0,1),pack(0,17)),17),
                         "unsupported")  # blended original vertex
        self.assertEqual(status((pack(2,1),pack(0,0),pack(1,17)),17),
                         "unsupported")  # indexed matrix blending
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"vertex_stage.csv"
            self.write_log(p,[
                ("model2_d3d9_vb_present_frame",301,17,1,0),
                ("model2_d3d9_vb_source",301,0x44,pack(1,0x144),pack(2048,28)),
                ("model2_d3d9_source_vertex_pipeline",301,*good),
                ("model2_d3d9_vb_present_frame",302,17,1,0),
                ("model2_d3d9_vb_source",302,0x44,pack(1,0x144),pack(2048,28)),
                ("model2_d3d9_source_vertex_pipeline",302,
                 pack(2,1),pack(0,0),pack(0,18)),
            ])
            src=REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(src["original_source_vertex_pipeline_status"],
                             ["present_mismatch","supported"])
            self.assertFalse(src["world_pass_proven"])

    def test_original_depth_cull_same_witness_never_world_proof(self):
        check=REPORT.original_d3d9_depth_cull_status
        good=(pack(1,7),pack(1,3),17)
        raster=(pack(3,7),pack(1,0),pack(0,0))
        self.assertEqual(check(None,17),"unobserved")
        self.assertEqual(check(good,17,raster),"supported")
        self.assertEqual(check(good,18,raster),"present_mismatch")
        self.assertEqual(check(good,17,(pack(3,4),pack(1,0),0)),
                         "source_raster_mismatch")
        self.assertEqual(check((pack(2,7),pack(1,3),17),17),"unsupported")
        self.assertEqual(check((pack(1,1),pack(1,3),17),17),"unsupported")
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/"source_depth.csv"
            self.write_log(p,[
                ("model2_d3d9_vb_present_frame",101,17,1,0),
                ("model2_d3d9_vb_source",101,0x44,pack(1,0x144),pack(2048,28)),
                ("model2_d3d9_source_depth_cull",101,*good),
                ("model2_d3d9_vb_present_frame",102,17,1,0),
                ("model2_d3d9_vb_source",102,0x44,pack(1,0x144),pack(2048,28)),
                ("model2_d3d9_source_depth_cull",102,
                 pack(1,7),pack(1,3),18),
            ])
            entry=REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(entry["original_source_depth_cull_status"],
                             ["present_mismatch","supported"])
            self.assertFalse(entry["world_pass_proven"])

    def test_source_mip_provenance_cannot_invent_world_or_lod(self):
        check=REPORT.original_d3d9_mip_status
        self.assertEqual(check(None,17),"unobserved")
        self.assertEqual(check((0,8,17),17),"mip_disabled")
        self.assertEqual(check((1,1,17),17),"single_level_equivalent")
        self.assertEqual(check((2,1,17),17),"single_level_equivalent")
        self.assertEqual(check((2,1,17),17,
                         (pack(1,1),pack(2,2),pack(2,0))),
                         "single_level_equivalent")
        self.assertEqual(check((2,1,17),17,
                         (pack(1,1),pack(2,2),pack(1,0))),
                         "sampler_state_mismatch")
        self.assertEqual(check((2,1,17),17,
                         (pack(1,1),pack(2,2),pack(2,1))),
                         "sampler_state_mismatch")
        self.assertEqual(check((2,2,17),17),"mip_unverified")
        self.assertEqual(check((2,0,17),17),"mip_unverified")
        self.assertEqual(check((2,1,18),17),"present_mismatch")

    def test_original_source_blend_requires_its_own_draw_witness(self):
        classify=REPORT.original_d3d9_blend_mode
        self.assertEqual(classify((pack(1,9),pack(1,1),0)),
                         "multiply_color")
        self.assertEqual(classify((pack(1,7),pack(8,1),0)),
                         "destination_alpha_over")
        self.assertEqual(classify((pack(1,1),pack(2,1),0)),
                         "preserve_destination")
        self.assertEqual(classify((pack(1,7),pack(8,1),1)),
                         "unsupported")
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/"original_blend.csv"
            self.write_log(path, [
                ("model2_d3d9_vb_present_frame",11,17,1,0),
                ("model2_d3d9_vb_source",11,0x1234,
                 pack(1,0x44),pack(600,20)),
                ("model2_d3d9_source_blend",11,
                 pack(1,7),pack(8,1),0),
                ("model2_d3d9_vb_present_frame",12,17,1,0),
                ("model2_d3d9_vb_source",12,0x1234,
                 pack(1,0x44),pack(600,20)),
                ("model2_d3d9_source_blend",12,
                 pack(1,7),pack(8,1),1),
                ("model2_d3d9_source_blend",13,
                 pack(1,9),pack(1,1),0),
            ])
            result=REPORT.audit_csv(path)
            source=result["sources"][0]
            self.assertEqual(source["original_source_blend_modes"],
                             ["destination_alpha_over"])
            self.assertEqual(
                source["original_source_blend_unsupported_witnesses"],1)
            self.assertEqual(result["world_pass_proof"],"NOT_ESTABLISHED")

    def test_material_must_match_its_own_original_draw_declaration(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "material.csv"
            self.write_log(p, [
                ("model2_d3d9_vb_present_frame", 10, 3, 1, 0),
                ("model2_d3d9_vb_source", 10, 0x44,
                 pack(1, 0x144), pack(2048, 28)),
                ("model2_d3d9_source_stage0_color", 10,
                 pack(4, 2), 0, pack(0x144, 24)), # stale stride
                ("model2_d3d9_source_stage0_alpha", 10,
                 pack(4, 2), 0, pack(0x144, 28)),
                ("model2_d3d9_source_stage1_specular", 10,
                 1, 1, pack(0, 0)),
            ])
            entry = REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(entry["original_material_complete_witnesses"], 0)
            self.assertEqual(entry["original_material_mismatched_witnesses"], 1)
            self.assertEqual(entry["original_material_stage_states"], [])
            self.assertEqual(
                entry["distinct_original_material_present_frame_count"], 0)
            self.assertEqual(REPORT.audit_csv(p)["world_pass_proof"],
                             "NOT_ESTABLISHED")

    def test_original_up_stage_is_not_a_fabricated_vb_orphan(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "both_families.csv"
            self.write_log(p, [
                ("model2_d3d9_vb_present_frame", 9, 3, 1, 0),
                ("model2_d3d9_vb_source", 9, 0x555,
                 pack(1, 0x104), pack(4096, 24)),
                ("model2_d3d9_source_stage0_color", 10,
                 pack(2, 2), 0, pack(0x144, 28)),
                ("model2_d3d9_source_stage0_alpha", 10,
                 pack(2, 2), 0, pack(0x144, 28)),
                ("model2_d3d9_source_stage1_specular", 10,
                 1, 1, 0),
                ("model2_d3d9_vb_stream", 11, 0, 0, 0),
            ])
            result = REPORT.audit_csv(p)
            self.assertEqual(result["caller_count"], 1)
            self.assertEqual(result["non_vb_material_witness_count"], 1)
            self.assertEqual(result["orphan_metadata_count"], 1)
            self.assertEqual(result["world_pass_proof"], "NOT_ESTABLISHED")

    def test_missing_material_stage_not_inferred_from_another_draw(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "material_partial.csv"
            self.write_log(p, [
                ("model2_d3d9_vb_present_frame", 10, 3, 1, 0),
                ("model2_d3d9_vb_source", 10, 0x44,
                 pack(1, 0x144), pack(2048, 28)),
                ("model2_d3d9_source_stage0_color", 11,
                 pack(4, 2), 0, pack(0x144, 28)),
                ("model2_d3d9_source_stage0_alpha", 10,
                 pack(4, 2), 0, pack(0x144, 28)),
                ("model2_d3d9_source_stage1_specular", 10,
                 1, 1, pack(0, 0)),
            ])
            entry = REPORT.audit_csv(p)["sources"][0]
            self.assertEqual(entry["original_material_complete_witnesses"], 0)
            self.assertEqual(entry["original_material_mismatched_witnesses"], 0)
            self.assertEqual(entry["original_material_stage_states"], [])
            self.assertEqual(REPORT.audit_csv(p)["world_pass_proof"],
                             "NOT_ESTABLISHED")

    def test_projected_material_candidates_require_exact_original_stage(self):
        # This is only fixed-function source evidence, never gameplay world
        # authorization; the original CPU camera/pass is still unknown.
        self.assertEqual(REPORT.original_stage_source(4, 2, 1), "MODULATE")
        self.assertEqual(REPORT.original_stage_source(2, 1, 2), "DIFFUSE")
        self.assertEqual(REPORT.original_stage_source(3, 0, 2), "TEXTURE")
        self.assertIsNone(REPORT.original_stage_source(4, 2, 3))
        self.assertIsNone(REPORT.original_stage_source(2, 0x11, 2))
        data = {
            "origin": (0x44, pack(1, 0x144), pack(2048, 28)),
            "stage_color": (pack(4, 2), 0, pack(0x144, 28)),
            "stage_alpha": (pack(4, 2), 0, pack(0x144, 28)),
            "stage_extra": (1, 1, pack(0, 0)),
            "stage_result": (1, 1, pack(0x144, 28)),
        }
        self.assertTrue(REPORT.original_projected_material_candidate(data))
        # Stage0 CURRENT=DIFFUSE is an exact D3D9 equivalence.
        data["stage_color"] = (pack(4, 2), 1, pack(0x144, 28))
        self.assertTrue(REPORT.original_projected_material_candidate(data))
        data["stage_color"] = (pack(4, 2), 3, pack(0x144, 28))
        self.assertFalse(REPORT.original_projected_material_candidate(data))
        data["stage_color"] = (pack(4, 0), 2, pack(0x144, 28))
        self.assertTrue(REPORT.original_projected_material_candidate(data))
        data["stage_extra"] = (1, 1, pack(1, 0))
        self.assertFalse(REPORT.original_projected_material_candidate(data))
        data["stage_extra"] = (1, 1, pack(0, 0))
        data["origin"] = (0x44, pack(1, 0x104), pack(2048, 24))
        data["stage_result"] = (1, 1, pack(0x104, 24))
        data["stage_color"] = (pack(4, 0), 2, pack(0x104, 24))
        data["stage_alpha"] = (pack(4, 2), 0, pack(0x104, 24))
        # Without explicit vertex D3DCOLOR, D3D9 defines diffuse=white.
        self.assertTrue(REPORT.original_projected_material_candidate(data))
        data["stage_color"] = (pack(4, 2), 1, pack(0x104, 24))
        self.assertTrue(REPORT.original_projected_material_candidate(data))
        data["stage_color"] = (pack(4, 2), 3, pack(0x104, 24))
        self.assertFalse(REPORT.original_projected_material_candidate(data))
        data["stage_color"] = (pack(4, 2), 0, pack(0x104, 24))
        data["stage_alpha"] = (pack(4, 1), 2, pack(0x104, 24))
        self.assertTrue(REPORT.original_projected_material_candidate(data))
        data["stage_color"] = (pack(2, 2), 0, pack(0x104, 24))
        data["stage_alpha"] = (pack(2, 2), 0, pack(0x104, 24))
        self.assertTrue(REPORT.original_projected_material_candidate(data))
        # Untextured RGB and alpha DIFFUSE are source-equivalent even when
        # stage-zero has no bound texture. A missing FVF diffuse still fails.
        data["origin"] = (0x44, pack(1, 0x144), pack(2048, 28))
        data["stage_result"] = (1, 1, pack(0x144, 28))
        data["stage_color"] = (pack(2, 0), 2, pack(0x144, 28))
        data["stage_alpha"] = (pack(3, 2), 0, pack(0x144, 28))
        self.assertTrue(REPORT.original_projected_material_candidate(data))
        data["stage_result"] = (2, 1, pack(0x144, 28)) # TEMP output
        self.assertFalse(REPORT.original_projected_material_candidate(data))
        data["stage_result"] = (1, 1, pack(0x144, 28))
        data["stage_alpha"] = (pack(3, 2), 0, pack(0x104, 24))
        self.assertFalse(REPORT.original_projected_material_candidate(data))
        # Real projected TEX0 packed DIFFUSE: exactly 20 bytes, no UV.
        data["origin"] = (0x44, pack(1, 0x44), pack(2048, 20))
        data["stage_result"] = (1, 1, pack(0x44, 20))
        data["stage_color"] = (pack(2, 0), 2, pack(0x44, 20))
        data["stage_alpha"] = (pack(2, 1), 0, pack(0x44, 20))
        self.assertTrue(REPORT.original_projected_material_candidate(data))
        data["stage_color"] = (pack(2, 2), 0, pack(0x44, 20))
        self.assertFalse(REPORT.original_projected_material_candidate(data))
        data["stage_color"] = (pack(2, 0), 2, pack(0x44, 20))
        data["present_frame"] = (22, 1, 0)
        data["source_mask"] = (10, pack(0x44, 20), 22)
        self.assertTrue(REPORT.original_projected_material_candidate(data))
        data["source_mask"] = (0, pack(0x44, 20), 22)
        self.assertFalse(REPORT.original_projected_material_candidate(data))
        data["source_mask"] = (10, pack(0x44, 20), 23)
        self.assertFalse(REPORT.original_projected_material_candidate(data))
        data["source_mask"] = (10, pack(0x44, 20), 22)
        data["write_color"] = (pack(0, 1), 0, 0)
        self.assertFalse(REPORT.original_projected_material_candidate(data))
        data["write_color"] = (pack(0, 10), 0, 0)
        self.assertTrue(REPORT.original_projected_material_candidate(data))
        # Supported RGB blend must not unconditionally reject the genuine
        # projected D3D9 material, even when separate-alpha is enabled.
        rgb=(pack(1,5),pack(6,1),0)
        data["source_blend"]=rgb
        self.assertTrue(REPORT.original_projected_material_candidate(data))
        data["source_blend"]=(pack(1,5),pack(6,1),1)
        self.assertFalse(REPORT.original_projected_material_candidate(data))
        data["source_separate_alpha_blend"]=(pack(2,1),1,pack(0x44,20))
        self.assertTrue(REPORT.original_projected_material_candidate(data))
        data["source_separate_alpha_blend"]=(pack(2,1),1,pack(0x44,24))
        self.assertFalse(REPORT.original_projected_material_candidate(data))
        del data["source_separate_alpha_blend"]
        data["source_mask"]=(2,pack(0x44,20),22)
        data["write_color"]=(pack(0,2),0,0)
        self.assertTrue(REPORT.original_projected_material_candidate(data))
        data["source_mask"]=(2,pack(0x44,20),23)
        self.assertFalse(REPORT.original_projected_material_candidate(data))

    def test_reject_corruption_and_never_infer_runtime_pass(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "probe.csv"
            self.write_log(p, [
                ("model2_d3d9_vb_source", 1, 0, pack(1, 0x112),
                 pack(2048, 20)),
            ])
            item = REPORT.audit_csv(p)["sources"][0]
            self.assertFalse(item["caller_is_in_executable"])
            self.assertEqual(item["distinct_present_frame_count"], 0)
            self.assertFalse(item["world_pass_proven"])
            self.assertEqual(REPORT.signed32(0xFFFFFFFE), -2)
            self.write_log(p, [
                ("model2_d3d9_vb_source", 1, 5, pack(1, 0x112),
                 pack(2048, 20)),
                ("model2_d3d9_vb_source", 1, 5, pack(1, 0x112),
                 pack(2048, 20)),
            ])
            with self.assertRaisesRegex(ValueError, "duplicate source witness"):
                REPORT.audit_csv(p)
            # A new matrix, geometry command, source context, or original
            # material under an already used witness is not another Draw.
            # It must fail closed, not silently overwrite the first capture.
            for event in (
                "model2_d3d9_cpu_vb_matrix_identity",
                "model2_d3d9_cpu_vb_matrix_content",
                "model2_d3d9_cpu_vb_geometry_command",
                "model2_d3d9_source_pass_context",
                "model2_d3d9_vb_present_frame",
                "model2_d3d9_source_stage0_color",
                "model2_d3d9_source_depth_cull",
                "model2_d3d9_source_fill",
            ):
                with self.subTest(event=event):
                    self.write_log(p, [
                        ("model2_d3d9_vb_source", 1, 5, pack(1, 0x112),
                         pack(2048, 20)),
                        (event, 1, 100, 200, 300),
                        (event, 1, 100, 200, 301),
                    ])
                    with self.assertRaisesRegex(
                            ValueError, "conflicting original Draw witness"):
                        REPORT.audit_csv(p)
            self.write_log(p, [
                ("model2_d3d9_vb_source", 1, 5, pack(1, 0x112),
                 pack(2048, 20)),
                # A valid matrix witness has mixed=0 or mixed=1; the old
                # synthetic value 200 violated the real original log ABI.
                ("model2_d3d9_cpu_vb_matrix_content", 1, 100, 0, 300),
                ("model2_d3d9_cpu_vb_matrix_content", 1, 100, 0, 300),
            ])
            self.assertEqual(REPORT.audit_csv(p)["caller_count"], 1)
            self.write_log(p, [
                ("model2_d3d9_vb_source", 0, 5, pack(1, 0x112),
                 pack(2048, 20)),
            ])
            with self.assertRaisesRegex(ValueError, "zero witness"):
                REPORT.audit_csv(p)
            self.write_log(p, [
                ("model2_d3d9_vb_source", 1, -1, pack(1, 0x112),
                 pack(2048, 20)),
            ])
            with self.assertRaisesRegex(ValueError, "malformed"):
                REPORT.audit_csv(p)

    def test_original_cpu_vertex_identity_cardinality_auditor(self):
        # New real x86 VB/IB event: two triangles with six distinct CPU
        # projector calls can remain passive candidates; three repeats
        # cannot promote an apparently complete GPU Draw.
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/"vertex_cardinality.csv"
            base=[
                ("model2_d3d9_vb_source",901,0x1234,pack(1,0x104),
                 pack(4096,24)),
                ("model2_d3d9_cpu_vb_join_batch",901,
                 pack(2,2),0,0x55),
            ]
            self.write_log(path,base+[(
                "model2_d3d9_cpu_vb_source_cardinality",901,6,0,0)])
            fresh=REPORT.audit_csv(path)["sources"][0]
            self.assertEqual(fresh["cpu_unique_source_vertex_batches"],1)
            self.assertEqual(fresh["cpu_aliased_source_vertex_batches"],0)
            self.assertEqual(fresh["cpu_fit_population_missing_batches"],1)
            population=[(
                "model2_d3d9_cpu_vb_source_cardinality",901,6,0,0),(
                "model2_d3d9_cpu_vb_fit_population",901,6,6,0)]
            self.write_log(path,base+population)
            joined=REPORT.audit_csv(path)["sources"][0]
            self.assertEqual(joined["cpu_fit_population_attested_batches"],1)
            self.assertEqual(joined["cpu_fit_population_missing_batches"],0)
            fan_base=[base[0],(
                "model2_d3d9_cpu_vb_join_batch",901,pack(4,4),0,0x55)]
            fan_population=[(
                "model2_d3d9_cpu_vb_source_cardinality",901,6,6,0),(
                "model2_d3d9_cpu_vb_fit_population",901,6,12,6)]
            self.write_log(path,fan_base+fan_population)
            self.assertEqual(REPORT.audit_csv(path)["sources"][0][
                "cpu_fit_population_attested_batches"],1)
            for invalid in ((12,12,6),(6,12,5),(5,12,6),(6,11,6)):
                with self.subTest(invalid=invalid):
                    self.write_log(path,fan_base+[fan_population[0],(
                        "model2_d3d9_cpu_vb_fit_population",901,*invalid)])
                    with self.assertRaisesRegex(
                            ValueError,"inconsistent original CPU fit/eye"):
                        REPORT.audit_csv(path)
            self.write_log(path,base+population+[population[1]])
            with self.assertRaisesRegex(
                    ValueError,"duplicate original CPU fit population"):
                REPORT.audit_csv(path)
            self.write_log(path,base+[population[1]])
            with self.assertRaisesRegex(
                    ValueError,"orphan original CPU fit population"):
                REPORT.audit_csv(path)
            self.write_log(path,base)
            legacy=REPORT.audit_csv(path)["sources"][0]
            self.assertEqual(legacy["cpu_legacy_unverified_vertex_batches"],1)
            self.write_log(path,base+[(
                "model2_d3d9_cpu_vb_source_cardinality",901,3,3,0)])
            incomplete=REPORT.audit_csv(path)["sources"][0]
            self.assertEqual(
                incomplete["cpu_insufficient_source_vertex_batches"],1)
            for card in ((5,0,0),(7,0,0),(6,1,0),(6,0,1)):
                self.write_log(path,base+[(
                    "model2_d3d9_cpu_vb_source_cardinality",901,*card)])
                with self.subTest(card=card),self.assertRaisesRegex(
                    ValueError,"inconsistent original CPU source cardinality"):
                    REPORT.audit_csv(path)
            self.write_log(path,[base[0],(
                "model2_d3d9_cpu_vb_join_batch",901,pack(2,1),
                pack(1,1),0),(
                "model2_d3d9_cpu_vb_source_cardinality",901,3,0,1)])
            alias=REPORT.audit_csv(path)["sources"][0]
            self.assertEqual(alias["cpu_aliased_source_vertex_batches"],1)
            self.assertEqual(alias["cpu_unique_source_vertex_batches"],0)
            self.write_log(path,[base[0],(
                "model2_d3d9_cpu_vb_source_cardinality",901,6,0,0)])
            with self.assertRaisesRegex(ValueError,"orphan original CPU"):
                REPORT.audit_csv(path)
            self.write_log(path,base+[(
                "model2_d3d9_cpu_vb_source_cardinality",901,6,0,0),(
                "model2_d3d9_cpu_vb_source_cardinality",901,5,0,0)])
            with self.assertRaisesRegex(ValueError,"conflicting original Draw witness"):
                REPORT.audit_csv(path)
        join=(pack(2,2),0,0x55)
        self.assertEqual(REPORT.original_cpu_source_cardinality_status(
            join,(6,0,0)),"unique_source_vertices")
        self.assertEqual(REPORT.original_cpu_source_cardinality_status(
            join,None),"legacy_unverified")

    def test_actual_post_com_draw_completion_source_identity(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/"post_com_draw_source.csv"
            base=[
                ("model2_d3d9_vb_source",501,0x1234,pack(1,0x104),pack(4096,24)),
                ("model2_d3d9_vb_present_frame",501,17,1,0),
                ("model2_d3d9_source_draw_phase",501,17,12,pack(0x1234,1)),
                ("model2_d3d9_source_scene_phase",501,17,4,1),
                ("model2_d3d9_vb_original_result",501,0,0x1234,pack(17,1)),
                ("model2_d3d9_original_present_result",17,0,1,0),
            ]
            self.write_log(path,base+[(
                "model2_d3d9_source_draw_completion",501,17,12,15)])
            complete=REPORT.audit_csv(path)["sources"][0]
            self.assertEqual(complete["original_draw_completion_attested_witnesses"],1)
            self.assertEqual(complete["distinct_post_com_retained_present_frame_count"],1)
            # A complete original physical post-COM Draw can be traced
            # to its specific Present, while absent CPU/material joins
            # are never invented as candidate frame sets.
            self.assertEqual(complete["original_material_frame_lineage"]["post_com"], [17])
            self.assertEqual(complete["original_material_frame_lineage"]["scene_phase"], [])
            self.assertEqual(complete["original_material_frame_lineage"]["physical_world_owned"], [])
            self.assertEqual(complete["distinct_physical_world_owned_present_frame_count"], 0)
            self.assertEqual(complete["original_material_frame_lineage"]["projected"], [])
            for mask in (1,3,7,11,13):
                with self.subTest(mask=mask):
                    self.write_log(path,base+[(
                        "model2_d3d9_source_draw_completion",501,17,12,mask)])
                    partial=REPORT.audit_csv(path)["sources"][0]
                    self.assertEqual(partial["original_draw_completion_untrusted_witnesses"],1)
                    self.assertEqual(partial["distinct_post_com_retained_present_frame_count"],0)
            # A failed post-COM mask cannot contradict an independently
            # successful original HRESULT; this is inconsistent, not merely
            # a partially untrusted source.
            for mask in (0,14):
                self.write_log(path,base+[(
                    "model2_d3d9_source_draw_completion",501,17,12,mask)])
                contradiction=REPORT.audit_csv(path)["sources"][0]
                self.assertEqual(
                    contradiction["original_draw_completion_inconsistent_witnesses"],1)
            for frame,ordinal in ((18,12),(17,13)):
                with self.subTest(frame=frame,ordinal=ordinal):
                    self.write_log(path,base+[(
                        "model2_d3d9_source_draw_completion",501,frame,ordinal,15)])
                    stale=REPORT.audit_csv(path)["sources"][0]
                    self.assertEqual(stale["original_draw_completion_inconsistent_witnesses"],1)
            self.write_log(path,base+[(
                "model2_d3d9_source_draw_completion",501,17,12,16)])
            with self.assertRaisesRegex(ValueError,"invalid original physical Draw completion"):
                REPORT.audit_csv(path)
            self.write_log(path,base+[
                ("model2_d3d9_source_draw_completion",501,17,12,15),
                ("model2_d3d9_source_draw_completion",501,17,12,15)])
            with self.assertRaisesRegex(ValueError,"duplicate original physical Draw completion"):
                REPORT.audit_csv(path)

if __name__ == "__main__":
    unittest.main()
