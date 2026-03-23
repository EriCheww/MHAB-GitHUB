#include "commandregistry.h"

QVector<CommandDefinition> CommandRegistry::defaultCommands()
{
    return {
        {
            "Shutdown",
            "shutdown",
            "Safely shut down the balloon-side system",
            false,
            "",
            3,
            5
        },
        {
            "Tilt Telescope",
            "tilt_telescope",
            "Tilt the telescope to a target angle",
            true,
            "Angle (deg)",
            3,
            8
        },
        {
            "Capture Image",
            "capture_image",
            "Trigger image capture on payload side",
            false,
            "",
            3,
            5
        },
        {
            "Start Tracking",
            "start_tracking",
            "Start telescope tracking mode",
            false,
            "",
            3,
            5
        },
        {
            "Stop Tracking",
            "stop_tracking",
            "Stop telescope tracking mode",
            false,
            "",
            3,
            5
        }
    };
}