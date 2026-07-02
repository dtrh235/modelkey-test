#!/usr/bin/env python3
"""Patch stm32f407_lvglport.uvprojx for ui_v3-only build without corrupting XML."""
import xml.etree.ElementTree as ET
from pathlib import Path

PROJ = Path(__file__).resolve().parents[2] / "Projects" / "MDK-ARM" / "stm32f407_lvglport.uvprojx"

REMOVE_NAMES = {
    "custom.c", "events_init.c", "gui_guider.c", "setup_scr_screen.c", "widgets_init.c",
    "setup_scr_screen_1.c", "setup_scr_screen_2.c", "setup_scr_screen_3.c", "setup_scr_screen_4.c",
    "setup_scr_screen_5.c", "setup_scr_screen_6.c", "setup_scr_screen_7.c", "setup_scr_screen_8.c",
    "setup_scr_screen_9.c",
    "_2_alpha_82x77.c", "_3_alpha_240x320.c",
    "lv_font_montserratMedium_19.c", "lv_font_montserratMedium_51.c", "lv_font_SourceHanSerifSC_Regular_20.c",
    "lv_font_montserratMedium_12.c", "lv_font_montserratMedium_16.c", "lv_font_montserratMedium_18.c",
    "lv_font_SourceHanSerifSC_Regular_16.c", "lv_font_SourceHanSerifSC_Regular_18.c",
    "lv_font_SourceHanSerifSC_Regular_19.c", "lv_font_SourceHanSerifSC_Regular_21.c",
    "lv_font_SourceHanSerifSC_Regular_25.c",
    "lv_font_cn_wifi_21.c", "lv_font_cn_pair_21.c", "lv_font_cn_wifi_25.c", "lv_font_cn_pair_25.c",
    "lv_font_cn_home_20.c", "lv_font_cn_wifi_16.c",
    "app_screen_auth.c", "app_screen1_unlock.c", "app_screen2_login.c", "app_screen3_menu.c",
    "app_screen_pair_flow.c", "app_screen4_table.c", "app_screen5_flow.c", "app_screen6_dialog_base.c",
    "app_screen6_dialog_flow.c", "app_screen6_info.c", "app_screen7_flow.c", "app_screen8_flow.c",
    "app_screen8_focus.c", "app_screen8_popup.c", "app_screen9_10_flow.c", "app_screen_wifi_flow.c",
    "app_touch_ui.c", "app_nav_entries.c", "app_home_nav_btns.c",
    "ui.c", "ui_auth_input.c", "ui_flow_panels.c", "ui_menu_popup_utils.c", "ui_nav_flow.c", "ui_screen8_utils.c",
}

ADD_USER = [
    ("app_ui_v3.c", r"..\..\User\ui_v3\app_ui_v3.c"),
    ("app_ui_v3_theme.c", r"..\..\User\ui_v3\app_ui_v3_theme.c"),
    ("app_ui_v3_anim.c", r"..\..\User\ui_v3\app_ui_v3_anim.c"),
    ("app_ui_v3_widgets.c", r"..\..\User\ui_v3\app_ui_v3_widgets.c"),
    ("app_ui_v3_gesture.c", r"..\..\User\ui_v3\app_ui_v3_gesture.c"),
    ("app_ui_v3_screens.c", r"..\..\User\ui_v3\app_ui_v3_screens.c"),
    ("app_ui_v3_keys.c", r"..\..\User\ui_v3\app_ui_v3_keys.c"),
    ("app_ui_v3_users.c", r"..\..\User\ui_v3\app_ui_v3_users.c"),
    ("app_host_hw_selftest.c", r"..\..\User\app_host_hw_selftest.c"),
    ("app_legacy_ui_stubs.c", r"..\..\User\app_legacy_ui_stubs.c"),
    ("app_user_add_flow.c", r"..\..\User\app_user_add_flow.c"),
]

ADD_GUIDER = [
    ("lv_font_ui_v3_13.c", r"..\..\guider\generated\guider_fonts\lv_font_ui_v3_13.c"),
    ("lv_font_ui_v3_16.c", r"..\..\guider\generated\guider_fonts\lv_font_ui_v3_16.c"),
    ("lv_font_ui_v3_20.c", r"..\..\guider\generated\guider_fonts\lv_font_ui_v3_20.c"),
    ("lv_font_ui_v3_num_36.c", r"..\..\guider\generated\guider_fonts\lv_font_ui_v3_num_36.c"),
]


def make_file(name: str, path: str) -> ET.Element:
    fe = ET.Element("File")
    ET.SubElement(fe, "FileName").text = name
    ET.SubElement(fe, "FileType").text = "1"
    ET.SubElement(fe, "FilePath").text = path
    return fe


def main() -> None:
    tree = ET.parse(PROJ)
    root = tree.getroot()

    for inc in root.iter("IncludePath"):
        text = inc.text or ""
        if "ui_v3" not in text:
            inc.text = text.replace(r"..\..\User;", r"..\..\User;..\..\User\ui_v3;", 1)

    removed = 0
    for files in root.iter("Files"):
        for f in list(files.findall("File")):
            name = f.findtext("FileName")
            if name in REMOVE_NAMES:
                files.remove(f)
                removed += 1

    existing = {
        f.findtext("FileName")
        for g in root.iter("Group")
        for f in g.findall(".//File")
        if f.findtext("FileName")
    }

    added = 0
    for group in root.iter("Group"):
        gname = group.findtext("GroupName")
        files = group.find("Files")
        if files is None:
            continue
        if gname == "User":
            todo = ADD_USER
        elif gname == "guider":
            todo = ADD_GUIDER
        else:
            continue
        for name, path in todo:
            if name not in existing:
                files.append(make_file(name, path))
                existing.add(name)
                added += 1

    xml_body = ET.tostring(root, encoding="utf-8")
    header = b"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\r\n"
    text = (header + xml_body + b"\r\n").decode("utf-8")
    text = text.replace("\r\n", "\n").replace("\r", "\n").replace("\n", "\r\n")
    PROJ.write_bytes(text.encode("utf-8"))
    print(f"patched {PROJ.name}: removed={removed} added={added}")


if __name__ == "__main__":
    main()
