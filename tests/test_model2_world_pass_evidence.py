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
import os
import tempfile


def witness(cpu="0x004bb400", count=3):
    return {
        "draw_family": "DrawIndexedPrimitive",
        "caller_rva_hex": "0x004a1234",
        "caller_is_in_executable": True,
        "distinct_projected_material_present_frame_count": count,
        "distinct_successful_projected_material_present_frame_count": count,
        "original_draw_success_witnesses": count,
        "original_draw_success_by_physical_frame": [
            [frame, 1] for frame in range(1, count+1)],
        "original_successful_draw_ordinal_pairs": [
            [frame, 8+4*frame] for frame in range(1, count+1)],
        "original_present_success_ordinal_pairs": [
            [frame, 8+4*frame] for frame in range(1, count+1)],
        "original_draw_phase_attested_ordinal_pairs": [
            [frame, 8+4*frame] for frame in range(1, count+1)],
        "original_scene_phase_attested_ordinal_pairs": [
            [frame, 8+4*frame] for frame in range(1, count+1)],
        "original_scene_draw_ordinal_triplets": [
            [frame, 8+4*frame, 2 if frame == 2 else 1]
            for frame in range(1, count+1)],
        "original_draw_completion_attested_ordinal_pairs": [
            [frame, 8+4*frame] for frame in range(1, count+1)],
        "original_scene_material_postcom_ordinal_pairs": [
            [frame, 8+4*frame] for frame in range(1, count+1)],
        "original_present_success_by_physical_frame": [
            [frame, 1] for frame in range(1, count+1)],
        "original_draw_phase_attested_by_physical_frame": [
            [frame, 1] for frame in range(1, count+1)],
        "original_scene_phase_attested_by_physical_frame": [
            [frame, 1] for frame in range(1, count+1)],
        "original_draw_completion_attested_by_physical_frame": [
            [frame, 1] for frame in range(1, count+1)],
        "original_draw_phase_attested_witnesses": count,
        "original_scene_phase_attested_witnesses": count,
        "original_draw_completion_attested_witnesses": count,
        "original_present_success_witnesses": count,
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
        "distinct_physical_world_owned_present_frame_count": count,
        "original_source_vertex_pipeline_status": ["supported"],
        "original_source_fill_status": ["solid"],
        "original_source_depth_cull_status": ["supported"],
        "original_material_frame_lineage": {
            "physical_world_owned": list(range(1, count+1)),
            "projected": list(range(1, count+1)),
            "draw_success": list(range(1, count+1)),
            "present_success": list(range(1, count+1)),
            "draw_phase": list(range(1, count+1)),
            "scene_phase": list(range(1, count+1)),
            "post_com": list(range(1, count+1)),
            "same_witness_joint": list(range(1, count+1)),
            "main_context": list(range(1, count+1)),
        },
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
        "original_sampled_scene_draw_frame_budget": [[1,3],[2,3],[3,3]],
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
    @unittest.skipUnless(os.name == "nt", "requires real Windows file-share semantics")
    def test_direct_python_source_rejects_live_original_x86_writer(self):
        # Exercise the actual Win32 share-mode contract, not a mocked
        # boolean. No original game ROM, user capture, device or XR needed.
        import ctypes
        from ctypes import wintypes
        from tools import model2_world_pass_evidence as evidence

        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        create = kernel.CreateFileW
        create.argtypes = (
            wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, wintypes.LPVOID,
            wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE)
        create.restype = wintypes.HANDLE
        close = kernel.CloseHandle
        close.argtypes = (wintypes.HANDLE,)
        close.restype = wintypes.BOOL
        valid = report(witness())
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            original = root / "race_a.csv"
            original.write_bytes(b"synthetic original process PRIVATE source only")
            prior = original.read_bytes()
            cached = root / "PRIVATE_SOURCE_race_a.json"
            cached.write_bytes(b"previous private diagnostic preserved")
            # Real original x86 OpenLog owns a GENERIC_WRITE handle allowing
            # FILE_SHARE_READ (1) until game exit. An idle writer with no
            # append can otherwise pass before/after SHA-256 checks.
            live = create(str(original), 0x40000000, 1, None, 3, 0x80, None)
            self.assertNotIn(live, (None, ctypes.c_void_p(-1).value))
            try:
                with patch("tools.model2_world_pass_evidence.audit_csv",
                           return_value=valid) as parser:
                    with self.assertRaisesRegex(
                            ValueError, "ORIGINAL X86 CSV STILL OPEN"):
                        preflight_original_session(original)
                    parser.assert_not_called()
                    with redirect_stderr(io.StringIO()) as errors:
                        with self.assertRaises(SystemExit):
                            main(["--preflight", str(original),
                                  "--output", str(cached)])
                    self.assertIn("STILL OPEN FAIL-CLOSED", errors.getvalue())
                    parser.assert_not_called()

                # Direct seven-input CLI must refuse the same live source
                # BEFORE any cached report parsing or candidate promotion.
                controls_csv = {}
                for label in ("race_b", "title", "menu", "sky", "hud", "aux"):
                    path = root / (label + ".csv")
                    path.write_bytes(("synthetic " + label).encode())
                    controls_csv[label] = path
                args = ["--race", str(original), "--race",
                        str(controls_csv["race_b"])]
                for label in ("title", "menu", "sky", "hud", "aux"):
                    args += ["--control", label, str(controls_csv[label])]
                with redirect_stderr(io.StringIO()) as errors:
                    with self.assertRaises(SystemExit):
                        main(args + ["--output", str(cached)])
                self.assertIn("STILL OPEN FAIL-CLOSED", errors.getvalue())
                self.assertEqual(cached.read_bytes(),
                                 b"previous private diagnostic preserved")
                self.assertEqual(original.read_bytes(), prior)
            finally:
                self.assertTrue(close(live))

            # After producer exit, the same diagnostic-only source is
            # readable without modifying any original bytes or World3D.
            with patch("tools.model2_world_pass_evidence.audit_csv",
                       return_value=valid):
                checked = preflight_original_session(original)
            self.assertEqual(checked["world_pass_proof"], "NOT_ESTABLISHED")
            self.assertEqual(evidence._capture_digest(original),
                             evidence._capture_digest(original))
            self.assertEqual(original.read_bytes(), prior)

    @unittest.skipUnless(os.name == "nt", "requires genuine Win32 share-mode semantics")
    def test_original_x86_csv_read_lease_persists_through_preflight_publication(self):
        # A short closed-producer probe alone permits a new writer between
        # hash and Draw/Present parsing. Exercise actual Windows CreateFileW
        # sharing and rename denial, not a mocked source-closed flag.
        import ctypes
        from ctypes import wintypes
        from tools import model2_world_pass_evidence as evidence

        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        create = kernel.CreateFileW
        create.argtypes = (
            wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, wintypes.LPVOID,
            wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE)
        create.restype = wintypes.HANDLE
        close = kernel.CloseHandle
        close.argtypes = (wintypes.HANDLE,)
        close.restype = wintypes.BOOL
        invalid = (None, ctypes.c_void_p(-1).value)
        valid = report(witness())

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "race_a.csv"
            source.write_bytes(b"synthetic closed original Daytona x86 source")
            before = source.read_bytes()
            output = root / "PRIVATE_SOURCE_race_a.json"
            alternative = root / "replacement.csv"
            alternative.write_bytes(b"synthetic FORGED new process log")
            check_count = []

            def deny_writer_and_rename():
                # Writer can open this source AFTER a short probe, but must
                # NOT open it while any real source analysis lease is held.
                writer = create(str(source), 0x40000000, 1, None, 3, 0x80, None)
                if writer not in invalid:
                    close(writer)
                self.assertIn(writer, invalid)
                with self.assertRaises(OSError):
                    os.replace(alternative, source)
                self.assertTrue(alternative.exists())
                self.assertEqual(source.read_bytes(), before)
                check_count.append(True)

            def while_auditing(path):
                self.assertEqual(Path(path), source)
                deny_writer_and_rename()
                return valid

            real_publish = evidence._write_private_json_atomic
            def while_publishing(path, content, inputs):
                deny_writer_and_rename()
                return real_publish(path, content, inputs)

            with patch("tools.model2_world_pass_evidence.audit_csv",
                       side_effect=while_auditing), patch(
                       "tools.model2_world_pass_evidence._write_private_json_atomic",
                       side_effect=while_publishing):
                # Library and CLI paths both keep source immutable.
                result = preflight_original_session(source)
                self.assertEqual(result["world_pass_proof"], "NOT_ESTABLISHED")
                main(["--preflight", str(source), "--output", str(output)])
            self.assertEqual(len(check_count), 3)
            self.assertEqual(json.loads(output.read_text(encoding="utf-8")), valid)
            self.assertEqual(source.read_bytes(), before)

            # A closed transaction releases the source handle promptly.
            writer = create(str(source), 0x40000000, 1, None, 3, 0x80, None)
            self.assertNotIn(writer, invalid)
            self.assertTrue(close(writer))
            self.assertEqual(source.read_bytes(), before)

    @unittest.skipUnless(os.name == "nt", "requires genuine Win32 share-mode semantics")
    def test_original_seven_csv_read_leases_block_late_writer_until_candidate_published(self):
        import ctypes
        from ctypes import wintypes
        from tools import model2_world_pass_evidence as evidence
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        create = kernel.CreateFileW
        create.argtypes = (
            wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, wintypes.LPVOID,
            wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE)
        create.restype = wintypes.HANDLE
        close = kernel.CloseHandle
        close.argtypes = (wintypes.HANDLE,)
        close.restype = wintypes.BOOL
        invalid = (None, ctypes.c_void_p(-1).value)
        names = ("race_a", "race_b", "title", "menu", "sky", "hud", "aux")
        candidate = {"schema": 1, "world_pass_proof": "NOT_ESTABLISHED",
                     "hmd_runtime_validation": "UNTESTED",
                     "adapter_auto_enable": False}

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            paths = {name: root / (name + ".csv") for name in names}
            for name, path in paths.items():
                path.write_bytes(("synthetic original " + name).encode("ascii"))
            before = {name: path.read_bytes() for name, path in paths.items()}
            output = root / "PRIVATE_WORLD_CANDIDATES.json"
            argv = ["--race", str(paths["race_a"]), "--race", str(paths["race_b"])]
            for name in names[2:]:
                argv += ["--control", name, str(paths[name])]
            argv += ["--output", str(output)]
            verified = []

            def assert_all_held():
                for name, path in paths.items():
                    writer = create(str(path), 0x40000000, 1, None, 3, 0x80, None)
                    if writer not in invalid:
                        close(writer)
                    self.assertIn(writer, invalid,
                                  "source writer reopened during " + name)
                verified.append(True)

            def sample_report(path):
                assert_all_held()
                return {"source_kind": "synthetic diagnostic only"}

            real_publish = evidence._write_private_json_atomic
            def during_promotion(path, content, sources):
                assert_all_held()
                return real_publish(path, content, sources)

            with patch("tools.model2_world_pass_evidence._load_report",
                       side_effect=sample_report), patch(
                       "tools.model2_world_pass_evidence.compare_sessions",
                       return_value=candidate), patch(
                       "tools.model2_world_pass_evidence._write_private_json_atomic",
                       side_effect=during_promotion):
                main(argv)
            self.assertEqual(len(verified), 8)
            self.assertEqual(json.loads(output.read_text(encoding="utf-8")),candidate)
            for name, path in paths.items():
                self.assertEqual(path.read_bytes(), before[name])
                writer = create(str(path), 0x40000000, 1, None, 3, 0x80, None)
                self.assertNotIn(writer, invalid)
                self.assertTrue(close(writer))

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
            # A legitimate original --preflight must not destroy its own CSV
            # when the operator accidentally chooses it as --output.
            original_csv_bytes = csv.read_bytes()
            with patch("tools.model2_world_pass_evidence.audit_csv",
                       return_value=valid) as forbidden_audit:
                with redirect_stderr(io.StringIO()) as error:
                    with self.assertRaises(SystemExit):
                        main(["--preflight",str(csv),"--output",str(csv)])
                self.assertIn("must not overwrite",error.getvalue())
                forbidden_audit.assert_not_called()
            self.assertEqual(csv.read_bytes(),original_csv_bytes)
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

    def test_original_single_preflight_pins_csv_bytes_before_reporting(self):
        # The PowerShell collector already rehashes its source, but callers
        # invoking the standalone CLI/library directly also need this
        # boundary. Invented fixture, never real Daytona source material.
        from tools import model2_world_pass_evidence as evidence
        valid = report(witness())
        with tempfile.TemporaryDirectory() as dirname:
            folder = Path(dirname)
            csv = folder / "race_a.csv"
            csv.write_bytes(b"synthetic original x86 fixture BEFORE")
            output = folder / "PRIVATE_SOURCE_race_a.json"
            sentinel = b"prior validated private diagnostic only"
            output.write_bytes(sentinel)
            initial = csv.read_bytes()

            def mutate_during_audit(path):
                self.assertEqual(Path(path),csv)
                try:
                    csv.write_bytes(b"synthetic original x86 fixture AFTER")
                except OSError as exc:
                    # New Windows full-analysis read lease denies the write
                    # itself. Linux remains protected by before/after hashes.
                    raise ValueError(
                        "PRIVATE original x86 CSV write blocked during source audit") from exc
                return valid

            with patch("tools.model2_world_pass_evidence.audit_csv",
                       side_effect=mutate_during_audit) as observer:
                # Direct Python library callers must refuse the stale source
                # even though schema/Scene/camera/RT/Draw counters are valid.
                with self.assertRaisesRegex(
                        ValueError,"changed during single-session preflight|"
                                   "write blocked during source audit"):
                    evidence.preflight_original_session(csv)
            observer.assert_called_once()
            if os.name == "nt":
                self.assertEqual(initial,csv.read_bytes())
            else:
                self.assertNotEqual(initial,csv.read_bytes())
            self.assertEqual(output.read_bytes(),sentinel)

            # The CLI fails before publishing ANY report (existing output
            # remains unchanged), rather than merely refusing at collector
            # archive promotion time.
            csv.write_bytes(initial)
            with patch("tools.model2_world_pass_evidence.audit_csv",
                       side_effect=mutate_during_audit):
                with redirect_stderr(io.StringIO()) as err:
                    with self.assertRaises(SystemExit):
                        main(["--preflight",str(csv),"--output",str(output)])
            self.assertRegex(
                err.getvalue(),
                "changed during single-session preflight|write blocked during source audit")
            self.assertEqual(output.read_bytes(),sentinel)

            # Valid immutable original source is still diagnostic-only and
            # never starts World3D or claims HMD proof.
            csv.write_bytes(initial)
            with patch("tools.model2_world_pass_evidence.audit_csv",
                       return_value=valid):
                result=evidence.preflight_original_session(csv)
                self.assertEqual(result["world_pass_proof"],"NOT_ESTABLISHED")
                self.assertEqual(result["hmd_runtime_validation"],"UNTESTED")
                main(["--preflight",str(csv),"--output",str(output)])
            self.assertEqual(json.loads(output.read_text("utf-8")),valid)
            self.assertEqual(csv.read_bytes(),initial)

    def test_original_single_preflight_rejects_linked_original_csv(self):
        from tools import model2_world_pass_evidence as evidence
        with tempfile.TemporaryDirectory() as dirname:
            folder=Path(dirname)
            original=folder/"private-original.csv"
            original.write_bytes(b"synthetic untouched original")
            linked=folder/"linked-original.csv"
            try:
                linked.symlink_to(original)
            except (OSError,NotImplementedError):
                self.skipTest("symlink creation unavailable on test system")
            with patch("tools.model2_world_pass_evidence.audit_csv") as audit:
                with self.assertRaisesRegex(ValueError,"linked"):
                    evidence.preflight_original_session(linked)
            audit.assert_not_called()
            self.assertEqual(original.read_bytes(),b"synthetic untouched original")

    def test_direct_private_seven_session_cli_pins_bytes_and_strict_json(self):
        # This exercises the DIRECT candidate CLI before an archived receipt
        # exists, rather than re-testing the already-secured final verifier.
        # Every source here is invented synthetic metadata, NOT Daytona data.
        from tools import model2_world_pass_evidence as evidence
        with tempfile.TemporaryDirectory() as dirname:
            folder = Path(dirname)
            race_reports = [report(witness()), report(witness())]
            negative = controls()
            paths = {}
            for label, value in (
                ("race_a", race_reports[0]), ("race_b", race_reports[1]),
                *((label, negative[label]) for label in sorted(CONTROL_TYPES)),
            ):
                path = folder / (label + ".json")
                path.write_text(json.dumps(value), encoding="utf-8")
                paths[label] = path
            output = folder / "PRIVATE_DIRECT_CANDIDATES.json"
            argv = [
                "--race", str(paths["race_a"]),
                "--race", str(paths["race_b"]),
            ]
            for label in sorted(CONTROL_TYPES):
                argv.extend(("--control", label, str(paths[label])))
            argv.extend(("--output", str(output)))

            main(argv)
            accepted = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(len(accepted["candidates"]), 1)
            self.assertFalse(accepted["adapter_auto_enable"])
            self.assertEqual(accepted["world_pass_proof"], "NOT_ESTABLISHED")
            output.unlink()

            # A direct seven-stage candidate run must preserve EVERY
            # original source file when its output accidentally aliases
            # one of the private race/control JSON inputs.
            for stage in paths:
                with self.subTest(output_alias=stage):
                    source_before = paths[stage].read_bytes()
                    with redirect_stderr(io.StringIO()) as error:
                        with self.assertRaises(SystemExit):
                            main(argv[:-1] + [str(paths[stage])])
                    self.assertIn("must not overwrite",error.getvalue())
                    self.assertEqual(paths[stage].read_bytes(),source_before)
            self.assertFalse(output.exists())

            original = paths["race_a"].read_text(encoding="utf-8")
            for name, forged in (
                ("top_level_duplicate", original.replace(
                    '"schema": 1,', '"schema": 1, "schema": 1,', 1)),
                ("nested_draw_caller_duplicate", original.replace(
                    '"caller_rva_hex": "0x004a1234",',
                    '"caller_rva_hex": "0x004a1234", '
                    '"caller_rva_hex": "0x004a1234",', 1)),
                ("nonstandard_number", original.replace(
                    '"schema": 1,', '"schema": 1, "unknown": NaN,', 1)),
            ):
                with self.subTest(forgery=name):
                    self.assertNotEqual(original, forged)
                    paths["race_a"].write_text(forged, encoding="utf-8")
                    with redirect_stderr(io.StringIO()) as err:
                        with self.assertRaises(SystemExit):
                            main(argv)
                    self.assertIn("error:", err.getvalue())
                    self.assertFalse(output.exists())
            paths["race_a"].write_text(original, encoding="utf-8")

            # A concurrent collector can rewrite an EARLY original x86
            # source after it was SHA-pinned but while the LAST source
            # report is still loading. No stale candidate may be published.
            real_loader = evidence._load_report
            def late_original_rewrite(path):
                result = real_loader(path)
                if path == paths["aux"]:
                    paths["race_a"].write_text(original + " ",
                                               encoding="utf-8")
                return result
            with patch("tools.model2_world_pass_evidence._load_report",
                       side_effect=late_original_rewrite):
                with redirect_stderr(io.StringIO()) as err:
                    with self.assertRaises(SystemExit):
                        main(argv)
            self.assertIn("source changed during candidate analysis",
                          err.getvalue())
            self.assertFalse(output.exists())

            # Even an output link to a file OUTSIDE the seven inputs is
            # unsafe: refuse before analysis can follow it and overwrite
            # arbitrary local private operator data.
            linked_output = folder / "linked-output.json"
            unrelated = folder / "unrelated-private.txt"
            unrelated.write_text("keep this original private file",encoding="utf-8")
            try:
                linked_output.symlink_to(unrelated)
            except (OSError, NotImplementedError):
                pass  # Restricted Windows test account may block symlinks.
            else:
                with redirect_stderr(io.StringIO()) as error:
                    with self.assertRaises(SystemExit):
                        main(argv[:-1] + [str(linked_output)])
                self.assertIn("linked file",error.getvalue())
                self.assertEqual(unrelated.read_text(encoding="utf-8"),
                                 "keep this original private file")

            # Linked source paths are not independent original evidence.
            linked = folder / "linked-race-a.json"
            try:
                linked.symlink_to(paths["race_a"])
            except (OSError, NotImplementedError):
                pass  # Restricted Windows test account may block symlinks.
            else:
                with self.assertRaisesRegex(ValueError, "linked"):
                    evidence._load_report(linked)


    def test_direct_preflight_private_json_publish_is_failure_atomic(self):
        # The Windows collector has its own pending receipt, but the direct
        # --preflight CLI must not truncate an older PRIVATE diagnostic when
        # disk/rename publication fails. Synthetic fixture only.
        valid = report(witness())
        with tempfile.TemporaryDirectory() as dirname:
            folder = Path(dirname)
            original = folder / "original_x86.csv"
            original.write_bytes(b"synthetic original capture never overwritten")
            original_bytes = original.read_bytes()
            output = folder / "PRIVATE_SINGLE_SOURCE.json"
            previous = b"prior local PRIVATE preflight diagnostic"
            output.write_bytes(previous)
            with patch("tools.model2_world_pass_evidence.audit_csv",
                       return_value=valid):
                with patch("tools.model2_world_pass_evidence.os.replace",
                           side_effect=OSError("synthetic disk replace failure")):
                    with redirect_stderr(io.StringIO()) as error:
                        with self.assertRaises(SystemExit):
                            main(["--preflight", str(original),
                                  "--output", str(output)])
                    self.assertIn("synthetic disk replace failure",
                                  error.getvalue())
                self.assertEqual(output.read_bytes(), previous)
                self.assertEqual(original.read_bytes(), original_bytes)
                self.assertEqual(list(folder.glob("PRIVATE_PENDING_DIRECT_*")), [])
                main(["--preflight", str(original), "--output", str(output)])
            restored = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(restored, valid)
            self.assertEqual(restored["world_pass_proof"], "NOT_ESTABLISHED")
            self.assertEqual(original.read_bytes(), original_bytes)
            self.assertEqual(list(folder.glob("PRIVATE_PENDING_DIRECT_*")), [])

    def test_direct_seven_session_private_publish_is_failure_atomic(self):
        # Test the direct candidate CLI, not the previously completed
        # collector/receipt promotion. No actual game or PRIVATE bytes.
        with tempfile.TemporaryDirectory() as dirname:
            folder = Path(dirname)
            stages = ("race_a", "race_b", *sorted(CONTROL_TYPES))
            paths = {}
            for label in stages:
                data = report(witness()) if label.startswith("race") else report()
                data["synthetic_stage_identity"] = label
                path = folder / (label + ".json")
                path.write_text(json.dumps(data), encoding="utf-8")
                paths[label] = path
            before = {label: path.read_bytes() for label, path in paths.items()}
            argv = ["--race", str(paths["race_a"]),
                    "--race", str(paths["race_b"])]
            for label in sorted(CONTROL_TYPES):
                argv += ["--control", label, str(paths[label])]
            output = folder / "PRIVATE_DIRECT_CANDIDATES.json"
            previous = b"previous PRIVATE candidate diagnostic"
            output.write_bytes(previous)
            argv += ["--output", str(output)]
            with patch("tools.model2_world_pass_evidence.os.replace",
                       side_effect=OSError("synthetic candidate publish failure")):
                with redirect_stderr(io.StringIO()) as error:
                    with self.assertRaises(SystemExit):
                        main(argv)
                self.assertIn("synthetic candidate publish failure",
                              error.getvalue())
            self.assertEqual(output.read_bytes(), previous)
            self.assertEqual(list(folder.glob("PRIVATE_PENDING_DIRECT_*")), [])
            self.assertEqual(
                {label: path.read_bytes() for label, path in paths.items()}, before)
            main(argv)
            candidate = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(candidate["world_pass_proof"], "NOT_ESTABLISHED")
            self.assertFalse(candidate["adapter_auto_enable"])
            self.assertEqual(list(folder.glob("PRIVATE_PENDING_DIRECT_*")), [])

    def test_direct_preflight_rechecks_output_alias_before_atomic_publish(self):
        # A link inserted AFTER initial alias validation must never cause
        # the original x86 CSV to be overwritten at publication time.
        from tools import model2_world_pass_evidence as evidence
        valid = report(witness())
        with tempfile.TemporaryDirectory() as dirname:
            folder = Path(dirname)
            original = folder / "original.csv"
            original.write_bytes(b"untouched genuine-input-shaped synthetic bytes")
            output = folder / "PRIVATE_DIRECT_SOURCE.json"
            real_check = evidence._refuse_private_output_alias
            checks = [0]

            def introduce_late_link(destination, sources):
                checks[0] += 1
                if checks[0] == 2:
                    try:
                        destination.symlink_to(original)
                    except (OSError, NotImplementedError):
                        self.skipTest("symlinks unavailable on this Windows runner")
                return real_check(destination, sources)

            with patch("tools.model2_world_pass_evidence.audit_csv",
                       return_value=valid):
                with patch("tools.model2_world_pass_evidence._refuse_private_output_alias",
                           side_effect=introduce_late_link):
                    with redirect_stderr(io.StringIO()) as error:
                        with self.assertRaises(SystemExit):
                            main(["--preflight", str(original),
                                  "--output", str(output)])
            self.assertEqual(checks[0], 2)
            self.assertIn("linked file", error.getvalue())
            self.assertEqual(original.read_bytes(),
                             b"untouched genuine-input-shaped synthetic bytes")
            self.assertEqual(list(folder.glob("PRIVATE_PENDING_DIRECT_*")), [])

    def test_opt_in_original_race_readiness_requires_actual_same_draw_source(self):
        # These tests use synthetic records only. This option catches a
        # nonviable original race *before* collecting five expensive physical
        # negative-control sessions; it never authenticates World3D.
        with tempfile.TemporaryDirectory() as dirname:
            csv = Path(dirname) / "original-race.csv"
            csv.write_bytes(b"SYNTHETIC ONLY - patched preflight source")
            output_path = Path(dirname) / "private-source.json"
            viable = report(witness())
            with patch("tools.model2_world_pass_evidence.audit_csv",
                       return_value=viable):
                self.assertEqual(
                    preflight_original_session(csv, require_race_source=True),
                    viable)
                with io.StringIO() as output, redirect_stdout(output):
                    main(["--preflight", str(csv), "--require-race-source"])
                    captured = output.getvalue()
                    summary = json.loads(captured)
                self.assertEqual(
                    summary["race_source_preflight"],
                    "CANDIDATE_ONLY_NOT_AUTHORIZED")
                self.assertEqual(summary["world_pass_proof"], "NOT_ESTABLISHED")
                self.assertEqual(summary["hmd_runtime_validation"], "UNTESTED")
                self.assertNotIn("caller_rva_hex", captured)

            # A capture with valid Present + process nonce but ZERO original
            # race geometry cannot pass readiness. Ordinary diagnostic
            # preflight remains supported for the five nonrace controls.
            empty = report()
            with patch("tools.model2_world_pass_evidence.audit_csv",
                       return_value=empty):
                self.assertEqual(preflight_original_session(csv), empty)
                with redirect_stderr(io.StringIO()) as errors:
                    with self.assertRaises(SystemExit):
                        main(["--preflight", str(csv), "--require-race-source",
                              "--output", str(output_path)])
                self.assertIn("RACE_SOURCE_NOT_READY", errors.getvalue())
                self.assertFalse(output_path.exists())

            # A real camera write/lifetime witness must not be omitted even
            # when a synthetic same-Draw source record otherwise looks full.
            unsafe = report(witness())
            unsafe["original_physical_camera_com_lifetime"][
                "all_original_camera_com_lifetimes_trusted"] = False
            with patch("tools.model2_world_pass_evidence.audit_csv",
                       return_value=unsafe):
                with self.assertRaisesRegex(ValueError, "camera COM lifetime"):
                    preflight_original_session(csv, require_race_source=True)

            # Same-frame and exact original Scene ordinal integrity is
            # mandatory at the early race boundary, not merely at day-end.
            mismatched = report(witness())
            mismatched["sources"][0][
                "original_scene_draw_ordinal_triplets"][1][2] = 3
            with patch("tools.model2_world_pass_evidence.audit_csv",
                       return_value=mismatched):
                with self.assertRaisesRegex(ValueError, "Draw/Scene ordinal"):
                    preflight_original_session(csv, require_race_source=True)

            with redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit):
                    main(["--require-race-source"])

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

    def test_per_caller_physical_present_cardinality_rejects_forged_race(self):
        # Synthetic cached evidence may claim more DISTINCT physical frames
        # than the original x86 process actually presented. This must fail
        # before either caller can become a World3D candidate.
        for field in (
            "distinct_projected_material_present_frame_count",
            "distinct_successful_projected_material_present_frame_count",
            "distinct_physically_presented_material_present_frame_count",
            "distinct_phase_attested_physically_presented_material_present_frame_count",
            # Scene-attested is separately capped by the trusted scene
            # aggregate in the pre-existing stronger test below.
            "distinct_same_witness_joint_present_frame_count",
            "distinct_cpu_fit_population_present_frame_count",
            "distinct_context_present_frame_count",
        ):
            with self.subTest(field=field):
                race = report(witness())
                race["sources"][0][field] = 9  # only 8 physical Presents
                with self.assertRaisesRegex(ValueError, "source Present count"):
                    compare_sessions([race, report(witness())], controls())

    def test_source_causal_present_sets_cannot_grow_downstream(self):
        for field in (
            "distinct_successful_projected_material_present_frame_count",
            "distinct_physically_presented_material_present_frame_count",
            "distinct_phase_attested_physically_presented_material_present_frame_count",
            "distinct_scene_attested_physically_presented_material_present_frame_count",
        ):
            with self.subTest(field=field):
                race = report(witness())
                # The aggregate trusted Scene/Draw boundary must stay valid;
                # isolate an impossible same-source downstream frame set.
                race["original_physical_scene_draw_coverage"][
                    "trusted_scene_drawn_present_count"] = 5
                race["sources"][0][field] = 4  # 4 <= 8, but parent has 3
                decision = compare_sessions([race, report(witness())], controls())
                self.assertEqual(decision["candidates"], [])
                self.assertEqual(decision["rejected"][0]["reason"],
                                 "missing_same_draw_matrix_material_or_render_evidence")

    def test_original_preflight_rejects_forged_control_source_cardinality(self):
        forged = report(witness())
        forged["sources"][0]["distinct_context_present_frame_count"] = True
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "synthetic-original.csv"
            path.write_text("synthetic", encoding="utf-8")
            with patch("tools.model2_world_pass_evidence.audit_csv",
                       return_value=forged):
                with self.assertRaisesRegex(ValueError, "source Present count"):
                    preflight_original_session(path)

    def test_original_control_sample_cannot_outnumber_real_physical_presents(self):
        # One original primary-device Present may contribute at most one
        # source-control sample, even in an authentic-looking cached report.
        for label in ("title", "race"):
            forged = report(witness() if label == "race" else {})
            if label == "title":
                forged["sources"] = []
            forged["original_control_present_sample_count"] = 9
            forged["original_control_successful_present_count"] = 9
            forged["original_control_successful_drawn_present_count"] = 9
            with self.subTest(label=label):
                with self.assertRaisesRegex(ValueError,
                                            "physical control Present"):
                    if label == "race":
                        compare_sessions([forged, report(witness())], controls())
                    else:
                        compare_sessions([report(witness()), report(witness())],
                                         controls(title=forged))

    def test_original_scene_candidate_needs_camera_and_rt_present_coverage(self):
        # Four trusted Scene Draw frames cannot be elevated from two real
        # camera / RT0/depth drawn physical Presents of the same original
        # race process; the incomplete report remains usable for preflight
        # and rejected-caller diagnostics.
        for aggregate, field in (
            ("original_physical_camera_com_lifetime",
             "trusted_camera_drawn_present_count"),
            ("original_physical_draw_target_coverage",
             "trusted_backbuffer_present_count"),
        ):
            with self.subTest(aggregate=aggregate):
                race = report(witness())
                race[aggregate][field] = 2
                decision = compare_sessions(
                    [race, report(witness())], controls())
                self.assertFalse(decision["candidates"])
                self.assertEqual(
                    decision["rejected"][0]["reason"],
                    "race_scene_source_exceeds_camera_or_rt_present_coverage")
                with tempfile.TemporaryDirectory() as folder:
                    path = Path(folder) / "source.csv"
                    path.write_text("synthetic", encoding="utf-8")
                    with patch("tools.model2_world_pass_evidence.audit_csv",
                               return_value=race):
                        self.assertEqual(
                            preflight_original_session(path), race)

    def test_original_seven_session_comparison_rejects_extra_race_process(self):
        with self.assertRaisesRegex(ValueError,
                                    "exactly two independent original race"):
            compare_sessions(
                [report(witness()), report(witness()), report(witness())],
                controls())

    def test_original_draw_attestation_witnesses_bound_distinct_frames(self):
        # A cached report can name three different successfully presented
        # material frames despite observing only two or zero actual native
        # HRESULT/phase/Scene/completion Draw witnesses of this caller.
        cases = (
            ("original_draw_success_witnesses", 2),
            ("original_present_success_witnesses", 2),
            ("original_draw_phase_attested_witnesses", 2),
            ("original_scene_phase_attested_witnesses", 2),
            ("original_draw_completion_attested_witnesses", 2),
            ("original_draw_success_witnesses", True),
        )
        for field, forged_count in cases:
            with self.subTest(field=field, count=forged_count):
                race = report(witness())
                race["sources"][0][field] = forged_count
                decision = compare_sessions([race, report(witness())], controls())
                self.assertFalse(decision["candidates"])
                self.assertEqual(
                    decision["rejected"][0]["reason"],
                    "missing_same_draw_matrix_material_or_render_evidence")

    def test_original_draw_ordinals_cannot_outnumber_actual_phase_witnesses(self):
        # An ordinal is counted only from an original physical Draw/Scene
        # sample, even if several Draws occur within one Present.
        for ordinal, phase in (
            ("distinct_original_draw_ordinals",
             "original_draw_phase_attested_witnesses"),
            ("distinct_original_scene_ordinals",
             "original_scene_phase_attested_witnesses"),
        ):
            with self.subTest(ordinal=ordinal):
                race = report(witness())
                race["sources"][0][ordinal] = 4
                if ordinal == "distinct_original_scene_ordinals":
                    # Permit this ordinal RANGE; only the missing number
                    # of genuine native Scene witnesses may reject it.
                    race["sources"][0]["original_scene_ordinal_max"] = 4
                race["sources"][0][phase] = 3
                self.assertFalse(compare_sessions(
                    [race, report(witness())], controls())["candidates"])

    def test_original_post_com_success_witnesses_require_draw_success(self):
        # Even if physical material counts look consistent, a Present or
        # retained completion cannot have more sampled successes than the
        # authentic original game Draw HRESULT success count.
        for field in ("original_present_success_witnesses",
                      "original_draw_completion_attested_witnesses"):
            with self.subTest(field=field):
                race = report(witness())
                race["sources"][0][field] = 4
                decision = compare_sessions(
                    [race, report(witness())], controls())
                self.assertFalse(decision["candidates"])
                self.assertEqual(
                    decision["rejected"][0]["reason"],
                    "missing_same_draw_matrix_material_or_render_evidence")
                # Source preflight remains useful for rejected diagnostics,
                # never silently promoting these counts to VR ownership.
                with tempfile.TemporaryDirectory() as folder:
                    raw = Path(folder) / "synthetic.csv"
                    raw.write_text("synthetic", encoding="utf-8")
                    with patch("tools.model2_world_pass_evidence.audit_csv",
                               return_value=race):
                        self.assertEqual(preflight_original_session(raw), race)

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


    def test_original_caller_success_cannot_outnumber_complete_scene_draws(self):
        forged = witness()
        # The original physical x86 scene counted exactly nine attempted
        # Draws. A cached caller cannot claim ten successful COM Draws,
        # even when all of its frame sets are within actual Presents.
        forged["original_draw_success_witnesses"] = 10
        result = compare_sessions([report(forged), report(witness())],
                                  controls())
        self.assertFalse(result["candidates"])
        self.assertEqual(result["rejected"][0]["reason"],
                         "race_original_draw_witness_budget_exceeded")

    def test_original_distinct_gpu_callers_share_one_physical_draw_budget(self):
        first, second = witness(), witness()
        second["caller_rva_hex"] = "0x004a1235"
        # Each caller separately claims no more than the original nine
        # real Draws, but their disjoint observed successes cannot total 12.
        first["original_draw_success_witnesses"] = 6
        second["original_draw_success_witnesses"] = 6
        overclaimed = report(first, second)
        got = compare_sessions([overclaimed, report(witness())], controls())
        self.assertFalse(got["candidates"])
        self.assertEqual(
            {r["reason"] for r in got["rejected"]},
            {"race_original_draw_witness_budget_exceeded",
             "missing_in_race_capture"},
        )

    def test_original_phase_scene_and_completion_witness_budget(self):
        for field in (
            "original_draw_phase_attested_witnesses",
            "original_scene_phase_attested_witnesses",
        ):
            with self.subTest(field=field):
                forged = witness()
                forged[field] = 10
                got = compare_sessions([report(forged), report(witness())],
                                       controls())
                self.assertFalse(got["candidates"])
                self.assertEqual(
                    got["rejected"][0]["reason"],
                    "race_original_draw_witness_budget_exceeded")

    def test_original_scene_budget_accepts_exact_aggregate_successes(self):
        sources = []
        for offset in range(3):
            source = witness()
            source["caller_rva_hex"] = f"0x{0x004a1234 + offset:08x}"
            source["original_successful_draw_ordinal_pairs"] = [
                [frame, 8+4*frame+offset] for frame in (1, 2, 3)]
            source["original_scene_material_postcom_ordinal_pairs"] = [
                [frame, 8+4*frame+offset] for frame in (1, 2, 3)]
            source["original_present_success_ordinal_pairs"] = [
                [frame, 8+4*frame+offset] for frame in (1, 2, 3)]
            source["original_draw_phase_attested_ordinal_pairs"] = [
                [frame, 8+4*frame+offset] for frame in (1, 2, 3)]
            source["original_scene_phase_attested_ordinal_pairs"] = [
                [frame, 8+4*frame+offset] for frame in (1, 2, 3)]
            source["original_scene_draw_ordinal_triplets"] = [
                [frame, 8+4*frame+offset, 2 if frame == 2 else 1]
                for frame in (1, 2, 3)]
            source["original_draw_completion_attested_ordinal_pairs"] = [
                [frame, 8+4*frame+offset] for frame in (1, 2, 3)]
            source["original_draw_ordinal_min"] = 12 + offset
            source["original_draw_ordinal_max"] = 20 + offset
            sources.append(source)
        # Three nonoverlapping caller samples x three successful Draws
        # exactly match the nine real original physical Scene Draws.
        got = compare_sessions(
            [report(*copy.deepcopy(sources)), report(*copy.deepcopy(sources))],
            controls())
        self.assertEqual(len(got["candidates"]), 3)
        self.assertFalse(got["adapter_auto_enable"])




    def test_material_present_cannot_borrow_real_successes_from_other_frame(self):
        # The success count and original physical Draw budget still match,
        # but three material frames would borrow all three successful
        # original COM calls from frame 1 alone.
        source = witness()
        source["original_draw_success_by_physical_frame"] = [[1, 3]]
        first = report(source)
        first["original_sampled_scene_draw_frame_budget"] = [[1, 3]]
        got = compare_sessions([first, report(witness())], controls())
        self.assertEqual(got["candidates"], [])
        self.assertEqual(got["rejected"][0]["reason"],
                         "race_original_draw_witness_budget_exceeded")

    def test_multiple_authentic_original_draws_per_present_remain_valid(self):
        source = witness()
        source["original_draw_success_witnesses"] = 4
        source["original_draw_success_by_physical_frame"] = [
            [1, 2], [2, 1], [3, 1]]
        source["original_successful_draw_ordinal_pairs"] = [
            [1, 12], [1, 13], [2, 16], [3, 20]]
        source["original_present_success_ordinal_pairs"] = [
            [1, 12], [1, 13], [2, 16], [3, 20]]
        source["original_draw_phase_attested_ordinal_pairs"] = [
            [1, 12], [1, 13], [2, 16], [3, 20]]
        source["original_scene_phase_attested_ordinal_pairs"] = [
            [1, 12], [1, 13], [2, 16], [3, 20]]
        source["original_scene_draw_ordinal_triplets"] = [
            [1, 12, 1], [1, 13, 1], [2, 16, 2], [3, 20, 1]]
        source["original_draw_completion_attested_ordinal_pairs"] = [
            [1, 12], [1, 13], [2, 16], [3, 20]]
        source["distinct_original_draw_ordinals"] = 4
        # Every extra authentic Draw has a same-Present post-COM stage
        # witness, even when its CPU/material join was not sampled.
        for rows, counter in (
            ("original_present_success_by_physical_frame",
             "original_present_success_witnesses"),
            ("original_draw_phase_attested_by_physical_frame",
             "original_draw_phase_attested_witnesses"),
            ("original_scene_phase_attested_by_physical_frame",
             "original_scene_phase_attested_witnesses"),
            ("original_draw_completion_attested_by_physical_frame",
             "original_draw_completion_attested_witnesses"),
        ):
            source[rows] = [[1, 2], [2, 1], [3, 1]]
            source[counter] = 4
        got = compare_sessions([report(source), report(witness())], controls())
        self.assertEqual(len(got["candidates"]), 1)
        self.assertFalse(got["adapter_auto_enable"])

    def test_same_original_frame_success_budget_rejects_global_spare_capacity(self):
        source = witness()
        # The run has nine Draws, but physical Present 1 only has three.
        source["original_draw_success_by_physical_frame"] = [[1, 4]]
        source["original_draw_success_witnesses"] = 4
        got = compare_sessions([report(source), report(witness())], controls())
        self.assertFalse(got["candidates"])
        self.assertEqual(got["rejected"][0]["reason"],
                         "race_original_draw_witness_budget_exceeded")

    def test_distinct_original_callers_share_physical_frame_draw_budget(self):
        first, second = witness(), witness()
        second["caller_rva_hex"] = "0x004a1235"
        first["original_draw_success_by_physical_frame"] = [[1,2],[2,1]]
        second["original_draw_success_by_physical_frame"] = [[1,2],[3,1]]
        # Two disjoint callers claim four Draws on frame 1; actual stream
        # has three. Their aggregate six remains within the global nine.
        got = compare_sessions([report(first,second),
                                report(copy.deepcopy(first),copy.deepcopy(second))],
                               controls())
        self.assertFalse(got["candidates"])
        self.assertEqual({x["reason"] for x in got["rejected"]},
                         {"race_original_draw_witness_budget_exceeded"})

    def test_original_frame_draw_budget_requires_complete_bounded_provenance(self):
        for bad in (None, [[1,3],[1,3],[3,3]],
                    [[1,3],[2,True],[3,3]], [[1,3],[3,3],[2,3]],
                    [[1,3],[2,-1],[3,3]], [[1,3],[2,3],[9,3]]):
            with self.subTest(bad=bad):
                run = report(witness())
                if bad is None:
                    del run["original_sampled_scene_draw_frame_budget"]
                else:
                    run["original_sampled_scene_draw_frame_budget"] = bad
                got = compare_sessions([run,report(witness())],controls())
                self.assertFalse(got["candidates"])

    def test_original_frame_draw_counts_reject_missing_duplicate_or_unbalanced(self):
        for bad in (None, [[1,2],[2,1]], [[1,1],[1,1],[3,1]],
                    [[1,1],[2,False],[3,1]], [[1,1],[2,1],[9,1]],
                    [[2,1],[1,1],[3,1]]):
            with self.subTest(bad=bad):
                source = witness()
                if bad is None:
                    del source["original_draw_success_by_physical_frame"]
                else:
                    source["original_draw_success_by_physical_frame"] = bad
                got = compare_sessions([report(source),report(witness())],controls())
                self.assertFalse(got["candidates"])

    def test_original_post_com_stages_cannot_borrow_another_present(self):
        # Same totals and an authentic successful original COM Draw on all
        # material frames are insufficient when the physical Present,
        # phase, Scene or completion evidence is from another frame.
        fields = (
            "original_present_success_by_physical_frame",
            "original_draw_phase_attested_by_physical_frame",
            "original_scene_phase_attested_by_physical_frame",
            "original_draw_completion_attested_by_physical_frame",
        )
        for field in fields:
            with self.subTest(field=field):
                source = witness()
                source["original_draw_success_witnesses"] = 5
                source["original_draw_success_by_physical_frame"] = [
                    [1, 2], [2, 2], [3, 1]]
                # The global stage total remains three, but the stage
                # has no genuine original COM witness on material frame 3.
                source[field] = [[1, 2], [2, 1]]
                got = compare_sessions([report(source), report(witness())],
                                       controls())
                self.assertFalse(got["candidates"])
                self.assertEqual(got["rejected"][0]["reason"],
                                 "race_original_draw_witness_budget_exceeded")

    def test_original_post_com_stage_rows_are_complete_and_causal(self):
        fields = (
            "original_present_success_by_physical_frame",
            "original_draw_phase_attested_by_physical_frame",
            "original_scene_phase_attested_by_physical_frame",
            "original_draw_completion_attested_by_physical_frame",
        )
        bad_rows = (
            None, [[1, 1], [1, 2]], [[1, True], [2, 1], [3, 1]],
            [[1, 1], [2, 1]], [[1, 1], [2, 1], [9, 1]],
            [[3, 1], [2, 1], [1, 1]], [[1, 0], [2, 1], [3, 1]],
        )
        for field in fields:
            for rows in bad_rows:
                with self.subTest(field=field, rows=rows):
                    source = witness()
                    if rows is None:
                        del source[field]
                    else:
                        source[field] = rows
                    got = compare_sessions([report(source),report(witness())],
                                           controls())
                    self.assertFalse(got["candidates"])
                    self.assertEqual(got["rejected"][0]["reason"],
                                     "race_original_draw_witness_budget_exceeded")

    def test_original_same_frame_stage_predecessor_limits(self):
        # Each stage still totals three, but one Scene witness exists
        # without its own authentic Draw-phase witness on frame 3.
        source = witness()
        source["original_draw_success_witnesses"] = 5
        source["original_draw_success_by_physical_frame"] = [
            [1, 2], [2, 2], [3, 1]]
        source["original_draw_phase_attested_by_physical_frame"] = [
            [1, 1], [2, 1], [3, 1]]
        source["original_scene_phase_attested_witnesses"] = 4
        source["original_scene_phase_attested_by_physical_frame"] = [
            [1, 2], [2, 1], [3, 1]]
        got = compare_sessions([report(source),report(witness())],controls())
        self.assertFalse(got["candidates"])
        self.assertEqual(got["rejected"][0]["reason"],
                         "race_original_draw_witness_budget_exceeded")

    def test_original_stage_witnesses_keep_valid_extra_com_draw(self):
        source = witness()
        source["original_draw_success_witnesses"] = 4
        source["original_draw_success_by_physical_frame"] = [
            [1, 2], [2, 1], [3, 1]]
        source["original_successful_draw_ordinal_pairs"] = [
            [1, 12], [1, 13], [2, 16], [3, 20]]
        source["original_present_success_ordinal_pairs"] = [
            [1, 12], [1, 13], [2, 16], [3, 20]]
        source["original_draw_phase_attested_ordinal_pairs"] = [
            [1, 12], [1, 13], [2, 16], [3, 20]]
        source["original_scene_phase_attested_ordinal_pairs"] = [
            [1, 12], [1, 13], [2, 16], [3, 20]]
        source["original_scene_draw_ordinal_triplets"] = [
            [1, 12, 1], [1, 13, 1], [2, 16, 2], [3, 20, 1]]
        source["original_draw_completion_attested_ordinal_pairs"] = [
            [1, 12], [1, 13], [2, 16], [3, 20]]
        source["distinct_original_draw_ordinals"] = 4
        # Every extra authentic Draw has a same-Present post-COM stage
        # witness, even when its CPU/material join was not sampled.
        for rows, counter in (
            ("original_present_success_by_physical_frame",
             "original_present_success_witnesses"),
            ("original_draw_phase_attested_by_physical_frame",
             "original_draw_phase_attested_witnesses"),
            ("original_scene_phase_attested_by_physical_frame",
             "original_scene_phase_attested_witnesses"),
            ("original_draw_completion_attested_by_physical_frame",
             "original_draw_completion_attested_witnesses"),
        ):
            source[rows] = [[1, 2], [2, 1], [3, 1]]
            source[counter] = 4
        # Exactly one of two successful original Draws on frame 1 has
        # the full material witness chain. A spare successful Draw is
        # not automatically promoted to material.
        got = compare_sessions([report(source),report(witness())],controls())
        self.assertEqual(len(got["candidates"]),1)
        self.assertFalse(got["adapter_auto_enable"])

    def test_same_original_draw_ordinal_cannot_be_spent_by_two_callers(self):
        a, b = witness(), witness()
        b["caller_rva_hex"] = "0x004a1235"
        # Both callers fit the actual per-Present budget, but claim
        # the identical real Draw ordinal on the same original Present.
        result = compare_sessions(
            [report(a, b), report(copy.deepcopy(a), copy.deepcopy(b))],
            controls())
        self.assertFalse(result["candidates"])
        self.assertEqual(
            {r["reason"] for r in result["rejected"]},
            {"race_original_draw_witness_budget_exceeded"})

    def test_same_present_distinct_original_draw_ordinals_are_valid(self):
        a, b = witness(), witness()
        b["caller_rva_hex"] = "0x004a1235"
        for field in ("original_successful_draw_ordinal_pairs",
                      "original_present_success_ordinal_pairs",
                      "original_draw_phase_attested_ordinal_pairs",
                      "original_scene_phase_attested_ordinal_pairs",
                      "original_draw_completion_attested_ordinal_pairs",
                      "original_scene_material_postcom_ordinal_pairs"):
            b[field] = [[1, 13], [2, 17], [3, 21]]
        b["original_draw_ordinal_min"] = 13
        b["original_draw_ordinal_max"] = 21
        b["original_scene_draw_ordinal_triplets"] = [
            [1, 13, 1], [2, 17, 2], [3, 21, 1]]
        result = compare_sessions(
            [report(a, b), report(copy.deepcopy(a), copy.deepcopy(b))],
            controls())
        self.assertEqual(len(result["candidates"]), 2)
        self.assertFalse(result["adapter_auto_enable"])


    def test_original_scene_draw_identity_is_not_only_a_per_present_count(self):
        # Same material Draw and Scene per-Present counts survive, but the
        # cached source attached the genuine scene to a different Draw.
        src = witness()
        src["original_draw_success_witnesses"] = 4
        src["original_draw_success_by_physical_frame"] = [
            [1, 2], [2, 1], [3, 1]]
        src["original_successful_draw_ordinal_pairs"] = [
            [1, 12], [1, 13], [2, 16], [3, 20]]
        src["distinct_original_draw_ordinals"] = 4
        src["original_draw_phase_attested_witnesses"] = 4
        src["original_draw_phase_attested_by_physical_frame"] = [
            [1, 2], [2, 1], [3, 1]]
        src["original_draw_phase_attested_ordinal_pairs"] = [
            [1, 12], [1, 13], [2, 16], [3, 20]]
        positive = compare_sessions([report(src), report(witness())], controls())
        self.assertEqual(len(positive["candidates"]), 1)
        wrong = copy.deepcopy(src)
        wrong["original_scene_draw_ordinal_triplets"][0] = [1, 13, 1]
        got = compare_sessions([report(wrong), report(witness())], controls())
        self.assertFalse(got["candidates"])
        self.assertEqual(got["rejected"][0]["reason"],
                         "race_original_draw_witness_budget_exceeded")

    def test_original_scene_ordinal_triplets_strictly_validated(self):
        samples = (
            None,
            [[1, 12, 1], [1, 12, 1], [3, 20, 1]],
            [[1, 12, 1], [2, 16, True], [3, 20, 1]],
            [[1, 12, 1], [2, 16, 0], [3, 20, 1]],
            [[1, 12, 1], [2, 16, 100001], [3, 20, 1]],
            [[1, 12, 1], [3, 20, 1], [2, 16, 2]],
            [[1, 12, 1], [2, 16, 2]],
            [[1, 12, 1], [2, 16, 2], [9, 20, 1]],
            [[1, 12, 1], [2, 16, 2], [3, 21, 1]],
            [[1, 12, 1], [2, 16, 2], [3, 20, 1, 2]],
            [[1, 12, 1], [2, 16, 2], [3, 20, "1"]],
        )
        for malformed in samples:
            src = witness()
            if malformed is None:
                del src["original_scene_draw_ordinal_triplets"]
            else:
                src["original_scene_draw_ordinal_triplets"] = malformed
            got = compare_sessions([report(src), report(witness())],
                                   controls())
            self.assertFalse(got["candidates"], malformed)
            self.assertEqual(got["rejected"][0]["reason"],
                             "race_original_draw_witness_budget_exceeded")

    def test_original_scene_ordinal_range_is_derived_from_actual_draws(self):
        src = witness()
        # A cached report could previously retain plausible [1,2]
        # scene range while each Draw actually recorded only Scene #1.
        src["original_scene_draw_ordinal_triplets"] = [
            [1, 12, 1], [2, 16, 1], [3, 20, 1]]
        got = compare_sessions([report(src), report(witness())], controls())
        self.assertFalse(got["candidates"])
        self.assertEqual(got["rejected"][0]["reason"],
                         "race_original_draw_witness_budget_exceeded")

    def test_original_draw_ordinal_rows_missing_malformed_or_replayed(self):
        for bad in (
            None, [[1, 12], [1, 12], [3, 20]],
            [[1, 12], [2, True], [3, 20]],
            [[1, 12], [3, 20], [2, 16]],
            [[1, 12], [2, 16], [9, 20]],
            [[1, 12], [2, 16]],
            [[1, 12], [2, 16], [3, 11]],
            [[1, 12], [2, 16], [3, 99999999]],
        ):
            with self.subTest(bad=bad):
                src = witness()
                if bad is None:
                    del src["original_successful_draw_ordinal_pairs"]
                else:
                    src["original_successful_draw_ordinal_pairs"] = bad
                result = compare_sessions([report(src), report(witness())],
                                          controls())
                self.assertFalse(result["candidates"])
                self.assertEqual(result["rejected"][0]["reason"],
                                 "race_original_draw_witness_budget_exceeded")


    def test_stage_ordinal_pairs_cannot_borrow_another_draw_on_same_present(self):
        # Counts and material stage frames are unchanged, and original
        # Draw #13 genuinely succeeds on Present #1. Scene evidence for
        # #13 cannot make material Draw #12 Scene-attested.
        base = witness()
        base["original_draw_success_witnesses"] = 4
        base["original_draw_success_by_physical_frame"] = [
            [1, 2], [2, 1], [3, 1]]
        base["original_successful_draw_ordinal_pairs"] = [
            [1, 12], [1, 13], [2, 16], [3, 20]]
        base["distinct_original_draw_ordinals"] = 4
        # Both successful Draws on frame 1 were phase attested; only
        # Draw #12 has the later presented Scene/material chain.
        base["original_draw_phase_attested_witnesses"] = 4
        base["original_draw_phase_attested_by_physical_frame"] = [
            [1, 2], [2, 1], [3, 1]]
        base["original_draw_phase_attested_ordinal_pairs"] = [
            [1, 12], [1, 13], [2, 16], [3, 20]]
        for field in (
            "original_present_success_ordinal_pairs",
            "original_scene_phase_attested_ordinal_pairs",
            "original_draw_completion_attested_ordinal_pairs",
        ):
            base[field] = [[1, 12], [2, 16], [3, 20]]
        accepted = compare_sessions([report(base), report(witness())], controls())
        self.assertEqual(len(accepted["candidates"]), 1)
        for field in (
            "original_present_success_ordinal_pairs",
            "original_scene_phase_attested_ordinal_pairs",
            "original_draw_completion_attested_ordinal_pairs",
        ):
            forged = copy.deepcopy(base)
            forged[field][0] = [1, 13]
            got = compare_sessions([report(forged), report(witness())],
                                   controls())
            self.assertFalse(got["candidates"], field)
            self.assertEqual(got["rejected"][0]["reason"],
                             "race_original_draw_witness_budget_exceeded")

    def test_stage_ordinal_pairs_missing_replayed_or_bad_type_fail_closed(self):
        for field in (
            "original_present_success_ordinal_pairs",
            "original_draw_phase_attested_ordinal_pairs",
            "original_scene_phase_attested_ordinal_pairs",
            "original_draw_completion_attested_ordinal_pairs",
        ):
            for malformed in (
                None, [[1, 12], [1, 12], [3, 20]],
                [[1, 12], [2, True], [3, 20]],
                [[1, 12], [3, 20], [2, 16]],
                [[1, 12], [2, 16]],
                [[1, 12], [2, 16], [3, 21]],
                [[1, 12], [2, 16], [9, 20]],
            ):
                forged = witness()
                if malformed is None:
                    del forged[field]
                else:
                    forged[field] = malformed
                got = compare_sessions([report(forged), report(witness())],
                                       controls())
                self.assertFalse(got["candidates"], (field, malformed))
                self.assertEqual(got["rejected"][0]["reason"],
                                 "race_original_draw_witness_budget_exceeded")

    def test_stage_ordinal_causal_subset_is_exact_not_just_count(self):
        src = witness()
        src["original_draw_success_witnesses"] = 4
        src["original_draw_success_by_physical_frame"] = [
            [1, 2], [2, 1], [3, 1]]
        src["original_successful_draw_ordinal_pairs"] = [
            [1, 12], [1, 13], [2, 16], [3, 20]]
        src["distinct_original_draw_ordinals"] = 4
        # Both successful frame-1 Draws have authentic phase witnesses,
        # but only #13 has the Scene witness. Material #12 cannot borrow.
        src["original_draw_phase_attested_witnesses"] = 4
        src["original_draw_phase_attested_by_physical_frame"] = [
            [1, 2], [2, 1], [3, 1]]
        src["original_draw_phase_attested_ordinal_pairs"] = [
            [1, 12], [1, 13], [2, 16], [3, 20]]
        src["original_scene_phase_attested_ordinal_pairs"] = [
            [1, 13], [2, 16], [3, 20]]
        got = compare_sessions([report(src), report(witness())], controls())
        self.assertFalse(got["candidates"])
        self.assertEqual(got["rejected"][0]["reason"],
                         "race_original_draw_witness_budget_exceeded")

    def test_material_scene_postcom_must_be_same_original_draw_ordinal(self):
        for bad in (
            [[1, 12], [2, 16], [3, 19]],
            [[1, 12], [2, 16]],
            [[1, 12], [2, 16], [3, True]],
            [[1, 12], [2, 16], [2, 20]],
            [[1, 12], [2, 16], [3, 20], [3, 21]],
        ):
            with self.subTest(bad=bad):
                src = witness()
                src["original_scene_material_postcom_ordinal_pairs"] = bad
                result = compare_sessions([report(src), report(witness())],
                                          controls())
                self.assertFalse(result["candidates"])
                self.assertEqual(result["rejected"][0]["reason"],
                                 "race_original_draw_witness_budget_exceeded")

    def test_original_scene_camera_target_same_physical_present_required(self):
        # All independent physical coverage totals still equal three, but
        # the trusted camera/RT frame is from an unrelated physical Present.
        source = witness()
        source["original_material_frame_lineage"]["physical_world_owned"] = [1, 2, 4]
        got = compare_sessions([report(source), report(witness())], controls())
        self.assertFalse(got["candidates"])
        self.assertEqual(got["rejected"][0]["reason"],
                         "missing_same_draw_matrix_material_or_render_evidence")

    def test_original_physical_world_owned_frame_missing_or_invalid(self):
        for replacement in (None, [1, 2], [1, 1, 3],
                            [1, 2, True], [1, 2, 9]):
            with self.subTest(replacement=replacement):
                source = witness()
                if replacement is None:
                    del source["original_material_frame_lineage"]["physical_world_owned"]
                else:
                    source["original_material_frame_lineage"]["physical_world_owned"] = replacement
                got = compare_sessions([report(source), report(witness())],
                                       controls())
                self.assertFalse(got["candidates"])
                self.assertEqual(got["rejected"][0]["reason"],
                                 "missing_same_draw_matrix_material_or_render_evidence")

    def test_original_physical_world_owned_count_cannot_be_forged(self):
        # Impossible/boolean frame counts are rejected during preflight;
        # plausible-but-wrong counts remain diagnosable candidate failures.
        for wrong in (True, -1):
            with self.subTest(wrong=wrong):
                source = witness()
                source["distinct_physical_world_owned_present_frame_count"] = wrong
                with self.assertRaisesRegex(
                        ValueError, "per-caller source Present count"):
                    compare_sessions([report(source), report(witness())], controls())
        for wrong in (2, 4):
            with self.subTest(wrong=wrong):
                source = witness()
                source["distinct_physical_world_owned_present_frame_count"] = wrong
                got = compare_sessions([report(source), report(witness())],
                                       controls())
                self.assertFalse(got["candidates"])

    def test_original_frame_lineage_rejects_equal_count_cross_frame_swap(self):
        # Every old summary count remains three. The claimed successful
        # Draw frames are not a subset of the projected source frames.
        source = witness()
        source["original_material_frame_lineage"]["draw_success"] = [1, 2, 4]
        result = compare_sessions([report(source), report(witness())], controls())
        self.assertEqual(result["candidates"], [])
        self.assertEqual(result["rejected"][0]["reason"],
                         "missing_same_draw_matrix_material_or_render_evidence")

    def test_original_frame_lineage_rejects_scene_from_another_present(self):
        source = witness()
        source["original_material_frame_lineage"]["scene_phase"] = [1, 2, 4]
        result = compare_sessions([report(source), report(witness())], controls())
        self.assertEqual(result["candidates"], [])
        self.assertEqual(result["rejected"][0]["reason"],
                         "missing_same_draw_matrix_material_or_render_evidence")

    def test_original_scene_must_share_exact_frame_with_post_com_completion(self):
        source = witness()
        source["original_material_frame_lineage"]["post_com"] = [1, 2, 5]
        result = compare_sessions([report(source), report(witness())], controls())
        self.assertEqual(result["candidates"], [])
        self.assertEqual(result["rejected"][0]["reason"],
                         "missing_same_draw_matrix_material_or_render_evidence")

    def test_original_present_lineage_fails_closed_for_missing_malformed_frames(self):
        for wrong in (None, [1, 1, 2], [1, 2, 9], [1, 2, True],
                      [2, 1, 3], [1, 2], ["1", 2, 3]):
            with self.subTest(wrong=wrong):
                source = witness()
                if wrong is None:
                    del source["original_material_frame_lineage"]
                else:
                    source["original_material_frame_lineage"]["projected"] = wrong
                got = compare_sessions([report(source), report(witness())],
                                       controls())
                self.assertFalse(got["candidates"])
                self.assertEqual(got["rejected"][0]["reason"],
                                 "missing_same_draw_matrix_material_or_render_evidence")

    def test_original_valid_lineage_can_span_nonconsecutive_physical_frames(self):
        source = witness()
        for name in source["original_material_frame_lineage"]:
            source["original_material_frame_lineage"][name] = [1, 4, 7]
        # A genuine physical Present frame list and the original COM
        # successful Draw frames must be exactly coherent.
        for stage in (
            "original_draw_success_by_physical_frame",
            "original_present_success_by_physical_frame",
            "original_draw_phase_attested_by_physical_frame",
            "original_scene_phase_attested_by_physical_frame",
            "original_draw_completion_attested_by_physical_frame",
        ):
            source[stage] = [[1, 1], [4, 1], [7, 1]]
        race_a, race_b = report(source), report(source)
        for field in ("original_successful_draw_ordinal_pairs",
                      "original_present_success_ordinal_pairs",
                      "original_draw_phase_attested_ordinal_pairs",
                      "original_scene_phase_attested_ordinal_pairs",
                      "original_draw_completion_attested_ordinal_pairs",
                      "original_scene_material_postcom_ordinal_pairs"):
            source[field] = [[1, 12], [4, 16], [7, 20]]
        source["original_scene_draw_ordinal_triplets"] = [
            [1, 12, 1], [4, 16, 2], [7, 20, 1]]
        for run in (race_a, race_b):
            run["original_sampled_scene_draw_frame_budget"] = [
                [1, 3], [4, 3], [7, 3]]
        got = compare_sessions([race_a, race_b], controls())
        self.assertEqual(len(got["candidates"]), 1)
        self.assertFalse(got["adapter_auto_enable"])

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
        # A zero executable caller cannot appear in an authenticated
        # original all-family race inventory, even as a rejected candidate.
        with self.assertRaisesRegex(ValueError,
                                    "sampled original Draw absent from exact"):
            compare_sessions([report(unknown), report(unknown)], controls())

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

    def test_race_cached_report_cannot_bypass_exact_original_callers(self):
        # Cached PRIVATE reports are accepted by the cross-session CLI;
        # a race must pass the same original four-family inventory gate
        # as the freshly preflighted run, not only the negative controls.
        good=report(witness())
        self.assertEqual(len(compare_sessions([good,report(witness())],
                                              controls())["candidates"]),1)
        mutations=[]
        missing=copy.deepcopy(good)
        del missing["original_all_family_exact_caller_inventory"]
        mutations.append(missing)
        overflow=copy.deepcopy(good)
        overflow["original_all_family_exact_caller_inventory"]["overflow"]=True
        mutations.append(overflow)
        stale=copy.deepcopy(good)
        stale["original_all_family_exact_caller_inventory"]["physical_present_count"]=7
        mutations.append(stale)
        unsampled=copy.deepcopy(good)
        unsampled["original_all_family_exact_caller_inventory"]["callers"]=["0x004f5000"]
        unsampled["original_all_family_exact_caller_inventory"]["unique_caller_count"]=1
        mutations.append(unsampled)
        forged=copy.deepcopy(good)
        forged["original_all_family_caller_bloom"]["lane0"] ^= 1
        mutations.append(forged)
        for bad in mutations:
            for race_position in (0,1):
                races=[good,report(witness())]
                races[race_position]=bad
                with self.subTest(position=race_position,bad=bad):
                    with self.assertRaises(ValueError):
                        compare_sessions(races,controls())

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
