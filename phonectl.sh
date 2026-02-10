#!/bin/sh
# phonectl.sh - Phone control toolkit for openclaw
# Wraps uinput_touch + ADB commands for full phone control
# Usage: phonectl.sh <command> [args...]

# Ensure dynamic libraries are found (needed for adb)
export LD_LIBRARY_PATH=/data/data/app.botdrop/files/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}

ADB_CMD="/data/data/app.botdrop/files/usr/bin/adb -s localhost:5555"
TOUCH="/data/local/tmp/uinput_touch"
SCREENSHOT_DIR="/sdcard"
UIDUMP="/sdcard/window_dump.xml"

adb_shell() {
    $ADB_CMD shell "$@" 2>&1
}

case "$1" in
    # === Touch & Gesture ===
    tap)
        # phonectl.sh tap <x> <y>
        adb_shell "$TOUCH tap $2 $3"
        ;;
    longpress)
        # phonectl.sh longpress <x> <y> [ms=1000]
        adb_shell "$TOUCH longpress $2 $3 $4"
        ;;
    swipe)
        # phonectl.sh swipe <x1> <y1> <x2> <y2> [ms=300]
        adb_shell "$TOUCH swipe $2 $3 $4 $5 $6"
        ;;
    scroll_up)
        # phonectl.sh scroll_up - scroll page up (swipe up)
        adb_shell "$TOUCH swipe 540 1600 540 800 400"
        ;;
    scroll_down)
        # phonectl.sh scroll_down - scroll page down (swipe down)
        adb_shell "$TOUCH swipe 540 800 540 1600 400"
        ;;

    # === Key Events ===
    key)
        # phonectl.sh key <home|back|power|volup|voldown|enter|menu>
        adb_shell "$TOUCH key $2"
        ;;
    home)
        adb_shell "am start -a android.intent.action.MAIN -c android.intent.category.HOME"
        ;;
    back)
        adb_shell "$TOUCH key back"
        ;;

    # === Screen Capture ===
    screenshot)
        # phonectl.sh screenshot [filename]
        FNAME="${2:-screen.png}"
        adb_shell "screencap -p ${SCREENSHOT_DIR}/${FNAME}"
        echo "Saved to ${SCREENSHOT_DIR}/${FNAME}"
        ;;
    screencap_local)
        # phonectl.sh screencap_local [local_path] - capture and pull to local
        LOCAL="${2:-/tmp/screen.png}"
        adb_shell "screencap -p ${SCREENSHOT_DIR}/_tmp_cap.png"
        $ADB_CMD pull "${SCREENSHOT_DIR}/_tmp_cap.png" "$LOCAL" 2>&1
        ;;

    # === UI Analysis ===
    uidump)
        # phonectl.sh uidump - dump UI hierarchy as XML
        adb_shell "uiautomator dump ${UIDUMP}" >/dev/null
        adb_shell "cat ${UIDUMP}"
        ;;
    uidump_text)
        # phonectl.sh uidump_text - extract visible text from UI
        adb_shell "uiautomator dump ${UIDUMP}" >/dev/null
        adb_shell "cat ${UIDUMP}" | grep -oE 'text="[^"]*"' | sed 's/text="//;s/"$//' | grep -v '^$'
        ;;
    find_text)
        # phonectl.sh find_text "text" - find element bounds by text
        adb_shell "uiautomator dump ${UIDUMP}" >/dev/null
        adb_shell "cat ${UIDUMP}" | grep -oE "text=\"[^\"]*${2}[^\"]*\"[^>]*bounds=\"\[[0-9,\]\[0-9\]*\]\"" 2>/dev/null
        ;;
    tap_text)
        # phonectl.sh tap_text "text" - find text on screen and tap its center
        adb_shell "uiautomator dump ${UIDUMP}" >/dev/null
        BOUNDS=$(adb_shell "cat ${UIDUMP}" | grep -oE "text=\"[^\"]*${2}[^\"]*\"[^>]*bounds=\"\[[0-9]*,[0-9]*\]\[[0-9]*,[0-9]*\]\"" | head -1 | grep -oE 'bounds="\[[0-9]*,[0-9]*\]\[[0-9]*,[0-9]*\]"' | grep -oE '[0-9]+')
        if [ -z "$BOUNDS" ]; then
            echo "ERROR: Text '${2}' not found on screen"
            exit 1
        fi
        # Parse bounds: x1 y1 x2 y2
        X1=$(echo "$BOUNDS" | sed -n '1p')
        Y1=$(echo "$BOUNDS" | sed -n '2p')
        X2=$(echo "$BOUNDS" | sed -n '3p')
        Y2=$(echo "$BOUNDS" | sed -n '4p')
        CX=$(( (X1 + X2) / 2 ))
        CY=$(( (Y1 + Y2) / 2 ))
        echo "Found '${2}' at center ($CX, $CY)"
        adb_shell "$TOUCH tap $CX $CY"
        ;;

    # === App Management ===
    launch)
        # phonectl.sh launch <package/activity>
        adb_shell "am start -n $2"
        ;;
    launch_pkg)
        # phonectl.sh launch_pkg <package> - launch app by package name
        adb_shell "monkey -p $2 -c android.intent.category.LAUNCHER 1"
        ;;
    open_url)
        # phonectl.sh open_url <url>
        adb_shell "am start -a android.intent.action.VIEW -d '$2'"
        ;;
    current_app)
        # phonectl.sh current_app - show current foreground app
        adb_shell "dumpsys activity activities" | grep -E "mResumedActivity|mFocusedActivity" | head -1
        ;;
    list_apps)
        # phonectl.sh list_apps - list installed packages
        adb_shell "pm list packages" | sed 's/package://'
        ;;

    # === Text Input ===
    type)
        # phonectl.sh type "text" - type text (works for ASCII only)
        # Use ADB broadcast method
        adb_shell "am broadcast -a ADB_INPUT_TEXT --es msg '$2'"
        ;;

    # === Utility ===
    wait)
        # phonectl.sh wait [seconds=2] - wait for UI to settle
        sleep "${2:-2}"
        ;;
    shell)
        # phonectl.sh shell "command" - run arbitrary adb shell command
        shift
        adb_shell "$@"
        ;;
    screen_size)
        echo "1080x2340"
        ;;

    # === Help ===
    help|*)
        cat << 'EOF'
phonectl.sh - Phone Control Toolkit for openclaw

TOUCH & GESTURES:
  tap <x> <y>                    Tap at coordinates
  longpress <x> <y> [ms]         Long press (default 1000ms)
  swipe <x1> <y1> <x2> <y2> [ms] Swipe gesture
  scroll_up                       Scroll page up
  scroll_down                     Scroll page down

KEY EVENTS:
  key <name>     Press key (home|back|power|volup|voldown|enter|menu)
  home           Press Home
  back           Press Back

SCREEN:
  screenshot [name]     Capture screenshot to /sdcard/
  screencap_local [path] Capture and pull to local path

UI ANALYSIS:
  uidump              Dump full UI hierarchy XML
  uidump_text         Extract visible text from screen
  find_text "text"    Find element bounds by text content
  tap_text "text"     Find and tap on text

APPS:
  launch <pkg/act>    Launch activity
  launch_pkg <pkg>    Launch app by package
  open_url <url>      Open URL in browser
  current_app         Show foreground app
  list_apps           List installed packages

UTILITY:
  type "text"         Type text input
  wait [sec]          Wait for UI to settle
  shell "cmd"         Run arbitrary ADB shell command
  screen_size         Display screen resolution
  help                Show this help

SCREEN: 1080x2340
EOF
        ;;
esac
