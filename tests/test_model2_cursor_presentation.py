"""Genuine D3D9 hardware/GDI/emulated cursor has no implied OpenXR parity."""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class OriginalCursorStereoSourceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.probe = (ROOT/"src/probe/d3d9_probe.cpp").read_text("utf-8")
        cls.hooks = (ROOT/"src/probe/original_d3d9_cursor_hooks.inl").read_text("utf-8")
        cls.presentation = (ROOT/"src/probe/original_d3d9_presentation_hooks.inl").read_text("utf-8")
        cls.bridge = (ROOT/"src/probe/verified_world_bridge.cpp").read_text("utf-8")
        cls.lineage = (ROOT/"src/probe/original_d3d9_lineage.hpp").read_text("utf-8")
        cls.failed = (ROOT/"src/probe/original_d3d9_surface_result_gate.hpp").read_text("utf-8")
        cls.gpu = (ROOT/"src/probe/daytona_native_stereo.cpp").read_text("utf-8")

    def body(self, name):
        return self.hooks.split(name+"(",1)[1].split("\n}",1)[0]

    def test_original_com_10_11_12_hook_install_and_rollback(self):
        for slot, hook, original in (
                (10,"OriginalSetCursorPropertiesHook","g_original_set_cursor_properties"),
                (11,"OriginalSetCursorPositionHook","g_original_set_cursor_position"),
                (12,"OriginalShowCursorHook","g_original_show_cursor")):
            self.assertIn(f"Hook(vtable[{slot}]", self.probe)
            self.assertIn(f"reinterpret_cast<void*>(&{hook})", self.probe)
            self.assertIn(f"MH_DisableHook(vtable[{slot}]);MH_RemoveHook(vtable[{slot}]);",self.probe)
            self.assertIn(f"{original}=nullptr",self.probe)
        self.assertIn("&&cursor_image&&cursor_position&&cursor_visibility&&dialog",self.probe)
        self.assertIn('#include "original_d3d9_cursor_hooks.inl"',self.probe)

    def test_cursor_output_retires_both_eyes_before_original_call(self):
        for name,method,original in (
                ("OriginalSetCursorPropertiesHook",6,"g_original_set_cursor_properties(device,hotspot_x,hotspot_y,bitmap)"),
                ("OriginalSetCursorPositionHook",7,"g_original_set_cursor_position(device,x,y,flags)"),
                ("OriginalShowCursorHook",8,"g_original_show_cursor(device,show)")):
            body=self.body(name)
            self.assertLess(body.index(f"BeforeOriginalPresentationControl(rva,{method}u)"),
                            body.index(original,body.index(f"BeforeOriginalPresentationControl(rva,{method}u)")))
            self.assertIn("if(!g_primary_device.Owns(device))",body)
            self.assertIn("g_original_external_presentation_unproven.store(true",body)
            self.assertIn("BeforeOriginalPresentationControl(0u,5u)",body)
        self.assertIn("g.source_epoch.Revoke(physical_present)",self.bridge)
        self.assertIn("OriginalSourceFailure::OriginalPresentationStateUnproven",self.bridge)
        self.assertIn("g_original_cursor_visible.load(",self.probe)
        self.assertIn("g_original_cursor_unknown.load(",self.probe)
        self.assertIn("BeforeOriginalPresentationControl(0u,9u)",self.probe)

    def test_proven_hidden_cursor_moves_do_not_discard_racing_eyes(self):
        move=self.body("OriginalSetCursorPositionHook")
        self.assertIn("const bool may_draw=!visibility_known",move)
        self.assertIn("if(rva && may_draw)BeforeOriginalPresentationControl(rva,7u)",move)
        self.assertIn("if(may_draw)RecordOriginalLineageWrite(",move)
        self.assertIn("else if(!rva) {",move)
        self.assertIn("g_original_cursor_unknown.store(true",move)

    def test_actual_show_cursor_returns_previous_not_new_visibility(self):
        body=self.body("OriginalShowCursorHook")
        self.assertIn("const BOOL was_visible=g_original_show_cursor(device,show);",body)
        self.assertIn("g_original_cursor_visible.store(show!=FALSE",body)
        self.assertIn("return was_visible;",body)
        self.assertIn("(was_visible!=FALSE)!=visible_before",body)
        self.assertIn("g_original_cursor_visibility_known.store(true",body)
        self.assertIn("g_original_cursor_unknown.store(false",body)
        self.assertIn("g_original_cursor_visibility_known.store(false",self.probe)
        self.assertIn("g_original_cursor_unknown.store(unsafe",self.probe)

    def test_real_72_state_kinds_and_bounded_failure_codes(self):
        for field in ("DeviceCursorProperties=51","DeviceCursorPosition=52",
                      "DeviceCursorVisibility=53","kOriginalLineageKinds=61"):
            self.assertIn(field,self.lineage)
        self.assertIn("TrackFailedOriginalD3D9Surface(hr,71u,rva)",self.hooks)
        self.assertIn("method<=75u",self.failed)
        self.assertIn("method>75u",self.gpu)
        self.assertIn("method>9u",self.presentation)
        self.assertIn("method>9u",self.bridge)
        self.assertIn("method>=6u && method<=8u",self.bridge)
        self.assertIn("method==9u",self.bridge)
        self.assertIn("model2_d3d9_original_cursor_control_counts",self.probe)

    def test_private_read_only_auditors_and_no_game_payload(self):
        vb=(ROOT/"tools/model2_draw_source_report.py").read_text("utf-8")
        up=(ROOT/"tools/model2_up_draw_outcomes.py").read_text("utf-8")
        for audit in (vb,up):
            self.assertIn("model2_d3d9_original_presentation_control_rejected",audit)
            self.assertIn("(1,2,3,4,5,6,7,8,9)",audit)
            self.assertIn("(3,4,5,9)",audit)
            self.assertIn("invalid original presentation control rejection",audit)
        for forbidden in ("memcpy(","fwrite(","LogEvent(","DrawPrimitive(","World3D=true","bitmap->LockRect("):
            self.assertNotIn(forbidden,self.hooks)


if __name__=="__main__":
    unittest.main()
