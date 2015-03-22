#pragma pack(push)
#pragma pack(1)

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * This VI calibrates all active axes of the LStep control.
 */
void __cdecl LS4Calibrate(uInt8 *CommandExecuted);
/*!
 * This VI calibrates the selected axes.
 */
void __cdecl LS4CalibrateEx(uInt8 *X, uInt8 *Y, uInt8 *Z, 
	uInt8 *A, uInt8 *CommandExecuted);
/*!
 * This VI opens the connection to the LStep. The parameters loaded by 
 * LoadConfig are used to initialize the interface.
 */
void __cdecl LS4Connect(uInt8 *CommandExecuted);
/*!
 * 
 * This VI opens a connection to the LStep control. 
 * 
 * Interface selects the interface type:
 * 1 = RS232
 * 3 = ISA/DPRAM
 * 4 = PCI
 * 
 * set ComPort = 1 to select 'COM1'
 * 
 * BaudRate = 9600 or 57600 (RS232)
 * 
 * I/O Adress is the base adress of the LSTEP-PC card (ISA/DPRAM) 
 * 
 * Card Index is the zero-based index of the LStep-PCI card (PCI)
 * 
 */
void __cdecl LS4ConnectSimple(uInt32 Interface1RS2323ISA4PCI, 
	uInt32 COMPort, uInt16 Baudrate, uInt32 IOAdressISA, 
	uInt32 CardIndexPCI, uInt8 *ShowProtocol, uInt8 *InitializationOK);
/*!
 * This VI must be used to close the connection to the LStep control. It 
 * closes the interface and deallocates memory.
 */
void __cdecl LS4Disconnect(uInt8 *CommandExecuted);
/*!
 * Enables automatic retry when LStep commands fail, in particular for errors 
 * occuring in the VI WaitForAxisStop (see LStep documentation)
 */
void __cdecl LS4EnableCommandRetry(uInt8 *CommandRetry, 
	uInt8 *CommandExecuted);
/*!
 * Returns the value of an analog input.
 */
void __cdecl LS4GetAnalogInput(int32 Index, int32 *Value, 
	uInt8 *CommandExecuted);
/*!
 * This VI returns the values of some special inputs (only LSTEP-PC).
 */
void __cdecl LS4GetAnalogInputs2(int32 *PT100, int32 *MV, int32 *V24, 
	uInt8 *CommandExecuted);
/*!
 * This VI returns the state of the digital inputs. The return value should be 
 * converted to a boolean array.
 */
void __cdecl LS4GetDigitalInputs(int32 *Value, uInt8 *CommandExecuted);
/*!
 * This VI returns the state of the additional digital inputs (16-31). The 
 * return value should be converted to a boolean array.
 */
void __cdecl LS4GetDigitalInputsE(int32 *Value, uInt8 *CommandExecuted);
/*!
 * This VI reads the LStep encoder mask to detect active encoders.
 */
void __cdecl LS4GetEncoderMask(uInt8 *EncoderX, uInt8 *EncoderY, 
	uInt8 *EncoderZ, uInt8 *EncoderA, uInt8 *CommandExecuted);
/*!
 * This VI returns the LStep error code (--> LStep documentation)
 */
void __cdecl LS4GetError(int32 *ErrorCode, uInt8 *CommandExecuted);
/*!
 * This VI returns the actual position of all axes.
 */
void __cdecl LS4GetPos(double *X, double *Y, double *Z, double *A, 
	uInt8 *CommandExecuted);
/*!
 * This VI returns the actual position of all axes. If encoder is set to 
 * 'true', the encoder values are returned.
 */
void __cdecl LS4GetPosEx(uInt8 *Encoder, uInt8 *CommandExecuted, 
	double *R, double *Z, double *Y, double *X);
/*!
 * This VI returns the position of a single axis (1..4).
 */
void __cdecl LS4GetPosSingleAxis(int32 Axis, double *Pos, 
	uInt8 *CommandExecuted);
/*!
 * LS4GetSerialNr
 */
void __cdecl LS4GetSerialNr(char SerialNr[], uInt8 *CommandExecuted, 
	int32 len);
/*!
 * This VI returns the snapshot count.
 */
void __cdecl LS4GetSnapshotCount(int32 *SnsCount, 
	uInt8 *CommandExecuted);
/*!
 * This VI returns the snapshap position.
 */
void __cdecl LS4GetSnapshotPos(double *X, double *Y, double *Z, double *A, 
	uInt8 *CommandExecuted);
/*!
 * This VI returns the snapshap position.
 */
void __cdecl LS4GetSnapshotPosArray(int32 Index, double *X, double *Y, 
	double *Z, double *A, uInt8 *CommandExecuted);
/*!
 * Returns a status string. ('OK...', 'ERR 1' etc.)
 */
void __cdecl LS4GetStatus(char Status[], int32 len);
/*!
 * Returns a string that describes the actual state of all axes.
 * 
 * Example:
 * StatusAxis = 'M@M@'
 * --> X and Z axis are moving
 * 
 */
void __cdecl LS4GetStatusAxis(char StatusAxisStr[], 
	uInt8 *CommandExecuted, int32 len);
/*!
 * LS4GetSwitches
 */
void __cdecl LS4GetSwitches(int32 *Flags, uInt8 *CommandExecuted);
/*!
 * This VI returns the LStep version number. (string)
 */
void __cdecl LS4GetVersionStr(char Vers[], uInt8 *CommandExecuted, 
	int32 len);
/*!
 * This VI returns the detailed LStep version number. (string)
 */
void __cdecl LS4GetVersionStrDet(char VersDet[], uInt8 *CommandExecuted, 
	int32 len);
/*!
 * Loads all LStep parameters (interface, axis parameters, controller..) from 
 * an INI-file. The format is compatible to the Wincom4-INI-file 
 * (Wincom4.ini).
 */
void __cdecl LS4LoadConfig(char FileName[], uInt8 *CommandExecuted);
/*!
 * LS4LStepSave
 */
void __cdecl LS4LStepSave(uInt8 *CommandExecuted);
/*!
 * Move absolute to the position. 
 * If 'Wait' is set to 'true', the VI waits till the position is reached.
 */
void __cdecl LS4MoveAbs(uInt8 *Wait, double A, double Z, double Y, 
	double X, uInt8 *CommandExecuted);
/*!
 * Move a single axis to the specified position. (1..4)
 * If 'Wait' is set to 'true', the VI waits till the position is reached.
 */
void __cdecl LS4MoveAbsSingleAxis(int32 Axis, double Value, 
	uInt8 *Wait, uInt8 *CommandExecuted);
/*!
 * Move absolute to the position. 
 * If 'Wait' is set to 'true', the VI waits till the position is reached.
 * AxisCount selects the number of axes to move (AxisCount=2 means: move X and 
 * Y)
 * If 'Relative' is set to 'true', the values are interpreted as a relative 
 * vector, otherwise as an absolute vector
 */
void __cdecl LS4MoveEx(double X, double Y, double Z, double A, 
	int32 AxisCount, uInt8 *Wait, uInt8 *Relative, 
	uInt8 *CommandExecuted);
/*!
 * Move relative to the position. 
 * If 'Wait' is set to 'true', the VI waits till the position is reached.
 */
void __cdecl LS4MoveRel(uInt8 *Wait, double A, double Z, double Y, 
	double X, uInt8 *CommandExecuted);
/*!
 * Move Relative Short (faster than LS4 MoveRel.vi). The distance can be set 
 * by LS4 SetDistance.vi.
 */
void __cdecl LS4MoveRelShort(uInt8 *CommandExecuted);
/*!
 * Move relative (single axis, 1..4)
 * If 'Wait' is set to 'true', the VI waits till the position is reached.
 */
void __cdecl LS4MoveRelSingleAxis(int32 Axis, double Value, 
	uInt8 *Wait, uInt8 *CommandExecuted);
/*!
 * This VI measures the table-travel for all axes.
 */
void __cdecl LS4RMeasure(uInt8 *CommandExecuted);
/*!
 * This VI measures the table-travel for the selected axes.
 */
void __cdecl LS4RMeasureEx(uInt8 *X, uInt8 *Y, uInt8 *Z, 
	uInt8 *A, uInt8 *CommandExecuted);
/*!
 * Saves all LStep parameters (interface, axis parameters, controller..) to an 
 * INI-file. The format is compatible to the Wincom4-INI-file (Wincom4.ini).
 */
void __cdecl LS4SaveConfig(char FileName[], uInt8 *CommandExecuted);
/*!
 * This VI can be used to bypass the LStep API functions. It directly sends a 
 * string to the LStep, and reads the answer if ReadLine is set to 'true'. 
 * TimeOut specifies the read timeout in ms.
 */
void __cdecl LS4SendString(char Str[], int32 TimeOut, uInt8 *ReadLine, 
	char Ret[], uInt8 *CommandExecuted, int32 len);
/*!
 * This VI can be used to bypass the LStep API functions. It directly sends a 
 * string to the LStep, and reads the answer if ReadLine is set to 'true'. 
 * TimeOut specifies the read timeout in ms.
 */
void __cdecl LS4SendStringPosCmd(char Str[], int32 TimeOut, 
	uInt8 *ReadLine, char Ret[], uInt8 *CommandExecuted, int32 len);
/*!
 * This VI aborts the communication with the LStep control. It can be useful 
 * to abort commands waiting for an answer of the LStep (MoveAbs, MoveRel...)
 */
void __cdecl LS4SetAbortFlag(uInt8 *CommandExecuted);
/*!
 * Sets the acceleration (m/s^2) of all axes.
 */
void __cdecl LS4SetAccel(double X, double Y, double Z, double A, 
	uInt8 *CommandExecuted);
/*!
 * Set the acceleration (m/s^2) of a single axis. (1..4)
 */
void __cdecl LS4SetAccelSingleAxis(int32 Axis, double Accel, 
	uInt8 *CommandExecuted);
/*!
 * This VI can be used to activate/deactivate axes.
 */
void __cdecl LS4SetActiveAxes(uInt8 *X, uInt8 *Y, uInt8 *Z, 
	uInt8 *A, uInt8 *CommandExecuted);
/*!
 * Sets the value of an analog output.
 */
void __cdecl LS4SetAnalogOutput(int32 Index, int32 Value, 
	uInt8 *CommandExecuted);
/*!
 * This VI sends the !autostatus command. (--> LStep documentation)
 */
void __cdecl LS4SetAutoStatus(int32 Value, uInt8 *CommandExecuted);
/*!
 * Sets the axis direction (separate for each axis)
 * 
 * 1 = normal
 * -1 = reverse
 */
void __cdecl LS4SetAxisDirection(int32 XD, int32 YD, int32 ZD, 
	int32 AD, uInt8 *CommandExecuted);
/*!
 * LS4SetCalibOffset
 */
void __cdecl LS4SetCalibOffset(double X, double Y, double Z, double A, 
	uInt8 *CommandExecuted);
/*!
 * Sets the axis direction for calibration (separate for each axis).
 * 
 * 1 = normal
 * -1 = reverse
 */
void __cdecl LS4SetCalibrateDir(int32 XD, int32 YD, int32 ZD, 
	int32 AD, uInt8 *CommandExecuted);
/*!
 * This VI sets the vector start delay [ms].
 */
void __cdecl LS4SetCommandTimeout(uInt32 TimeoutForReadCommands, 
	uInt8 *CommandExecuted);
/*!
 * This VI sets the controller/regulator mode of all axes.
 * 
 * 0 --> regulator off
 * 1 --> regulator off after positioning
 * 2 --> regulator always on
 */
void __cdecl LS4SetController(int32 XC, int32 YC, int32 ZC, int32 AC, 
	uInt8 *CommandExecuted);
/*!
 * This VI sets the controller call time [ms].
 */
void __cdecl LS4SetControllerCall(int32 CtrCall, 
	uInt8 *CommandExecuted);
/*!
 * This VI sets the controller factor.
 */
void __cdecl LS4SetControllerFactor(double X, double Y, double Z, double A, 
	uInt8 *CommandExecuted);
/*!
 * This VI sets the controller steps.
 */
void __cdecl LS4SetControllerSteps(double X, double Y, double Z, double A, 
	uInt8 *CommandExecuted);
/*!
 * This VI sets the controller timeout [ms].
 */
void __cdecl LS4SetControllerTimeout(int32 CtrTimeout, 
	uInt8 *CommandExecuted);
/*!
 * Sets the controller target window delay [ms].
 */
void __cdecl LS4SetControllerTWDelay(int32 CtrTWDelay, 
	uInt8 *CommandExecuted);
/*!
 * Sends the LStep parameters. These can be loaded by LS4 LoadConfig.vi or 
 * edited by LS4 AxisParsDlg.vi / LS4 ControllerParsDlg.vi.
 */
void __cdecl LS4SetControlPars(uInt8 *CommandExecuted);
/*!
 * Disables the correction table (see LStep API documentation)
 */
void __cdecl LS4SetCorrTblOff(uInt8 *CommandExecuted);
/*!
 * Enables the correction table (spline interpolation or 2-dimensional linear 
 * interpolation). The filename of the file containing the correction table 
 * data must be passed to the function (see LStep API documentation for 
 * details on the format of this file)
 */
void __cdecl LS4SetCorrTblOn(char FileName[], uInt8 *CommandExecuted);
/*!
 * Sets the current delay (separate for each axis)
 */
void __cdecl LS4SetCurrentDelay(int32 XD, int32 YD, int32 ZD, 
	int32 AD, uInt8 *CommandExecuted);
/*!
 * This VI sets the vector start delay [ms].
 */
void __cdecl LS4SetDelay(uInt32 Delay, uInt8 *CommandExecuted);
/*!
 * LS4SetDigIO_Distance
 */
void __cdecl LS4SetDigIO_Distance(int32 Index, uInt8 *Fkt, double Dist, 
	int32 Axis, uInt8 *CommandExecuted);
/*!
 * LS4SetDigIO_EmergencyStop
 */
void __cdecl LS4SetDigIO_EmergencyStop(int32 Index, 
	uInt8 *CommandExecuted);
/*!
 * LS4SetDigIO_Off
 */
void __cdecl LS4SetDigIO_Off(int32 Index, uInt8 *CommandExecuted);
/*!
 * LS4SetDigIO_Polarity
 */
void __cdecl LS4SetDigIO_Polarity(int32 Index, uInt8 *High, 
	uInt8 *CommandExecuted);
/*!
 * This VI sets a digital output (0..15) true or false.
 */
void __cdecl LS4SetDigitalOutput(int32 Index, int32 Value, 
	uInt8 *CommandExecuted);
/*!
 * This VI sets the states of the 16 digital outputs to true (1) oder false 
 * (0)
 * the states of the 16 digital outputs should be converted from a boolean 
 * array to an integer value that can be passed to the VI
 */
void __cdecl LS4SetDigitalOutputs(uInt32 Value, uInt8 *CommandExecuted);
/*!
 * This VI sets the states of the 16 additional digital outputs (16-31) to 
 * true (1) oder false (0)
 * the states of the 16 digital outputs should be converted from a boolean 
 * array to an integer value that can be passed to the VI
 */
void __cdecl LS4SetDigitalOutputsE(uInt32 Value, 
	uInt8 *CommandExecuted);
/*!
 * Selects the dimension of the position values (separate for each axis).
 * 
 * 0 = MicroSteps
 * 1 = um
 * 2 = mm
 */
void __cdecl LS4SetDimensions(int32 XD, int32 YD, int32 ZD, int32 AD, 
	uInt8 *CommandExecuted);
/*!
 * This VI sets the distance used by LS4 MoveRelShort.vi.
 */
void __cdecl LS4SetDistance(double X, double Y, double Z, double A, 
	uInt8 *CommandExecuted);
/*!
 * This VI activates/deactivate encoders.
 */
void __cdecl LS4SetEncoderActive(uInt8 *EncoderX, uInt8 *EncoderY, 
	uInt8 *EncoderZ, uInt8 *EncoderA, uInt8 *CommandExecuted);
/*!
 * This VI activates/deactivate encoders.
 */
void __cdecl LS4SetEncoderMask(uInt8 *EncoderX, uInt8 *EncoderY, 
	uInt8 *EncoderZ, uInt8 *EncoderA, uInt8 *CommandExecuted);
/*!
 * This VI sets the encoder periods.
 */
void __cdecl LS4SetEncoderPeriod(double X, double Y, double Z, double A, 
	uInt8 *CommandExecuted);
/*!
 * This VI can be used to select whether the GetPos and GetPosSingleAxis VIs 
 * should return encoder values.
 */
void __cdecl LS4SetEncoderPosition(uInt8 *Value, 
	uInt8 *CommandExecuted);
/*!
 * LS4SetEncoderRefSignal
 */
void __cdecl LS4SetEncoderRefSignal(int32 XR, int32 YR, int32 ZR, 
	int32 AR, uInt8 *CommandExecuted);
/*!
 * Move absolute to the position. 
 * If 'Wait' is set to 'true', the VI waits till the position is reached.
 */
void __cdecl LS4SetFactorMode(uInt8 *FactorMode, double A, double Z, 
	double Y, double X, uInt8 *CommandExecuted);
/*!
 * Sets factors for the pulse/direction mode.
 */
void __cdecl LS4SetFactorTVR(double X, double Y, double Z, double A, 
	uInt8 *CommandExecuted);
/*!
 * This VI sets gear factors for all axes
 * X = 4 --> Gear 1/4 
 */
void __cdecl LS4SetGear(double X, double Y, double Z, double A, 
	uInt8 *CommandExecuted);
/*!
 * LS4SetHandWheelOff
 */
void __cdecl LS4SetHandWheelOff(uInt8 *CommandExecuted);
/*!
 * LS4SetHandWheelOn
 */
void __cdecl LS4SetHandWheelOn(uInt8 *PositionCount, uInt8 *Encoder, 
	uInt8 *CommandExecuted);
/*!
 * LS4SetJoystickDir
 */
void __cdecl LS4SetJoystickDir(int32 XD, int32 YD, int32 ZD, 
	int32 AD, uInt8 *CommandExecuted);
/*!
 * LS4SetJoystickOff
 */
void __cdecl LS4SetJoystickOff(uInt8 *CommandExecuted);
/*!
 * LS4SetJoystickOn
 */
void __cdecl LS4SetJoystickOn(uInt8 *PositionCount, uInt8 *Encoder, 
	uInt8 *CommandExecuted);
/*!
 * Sets the joystick window (see LStep documentation)
 */
void __cdecl LS4SetJoystickWindow(uInt32 Value, uInt8 *CommandExecuted);
/*!
 * LS4SetLanguage
 */
void __cdecl LS4SetLanguage(char Language[], uInt8 *CommandExecuted);
/*!
 * LS4SetLimit
 */
void __cdecl LS4SetLimit(int32 Axis, double MinRange, double MaxRange, 
	uInt8 *CommandExecuted);
/*!
 * LS4SetLimitControl
 */
void __cdecl LS4SetLimitControl(int32 Axis, uInt8 *Active, 
	uInt8 *CommandExecuted);
/*!
 * LS4SetMotorCurrent
 */
void __cdecl LS4SetMotorCurrent(double X, double Y, double Z, double A, 
	uInt8 *CommandExecuted);
/*!
 * LS4SetPitch
 */
void __cdecl LS4SetPitch(double X, double Y, double Z, double A, 
	uInt8 *CommandExecuted);
/*!
 * LS4SetPos
 */
void __cdecl LS4SetPos(double X, double Y, double Z, double A, 
	uInt8 *CommandExecuted);
/*!
 * This VI enables/disables the windows message processing. By default, the 
 * LStep API processes messages in the UI thread while waiting for responses 
 * of LStep move commands.
 * 
 * Proc = -1 disables all message processing in the LStep API
 * Proc = 0 enables the standard message processing of the LStep API
 * Proc > 0 sets the callback pointer of a user defined callback function for 
 * message processing
 */
void __cdecl LS4SetProcessMessagesProc(uInt32 ProcedurePointer, 
	uInt8 *CommandExecuted);
/*!
 * LS4SetReduction
 */
void __cdecl LS4SetReduction(double X, double Y, double Z, double A, 
	uInt8 *CommandExecuted);
/*!
 * LS4SetRMOffset
 */
void __cdecl LS4SetRMOffset(double X, double Y, double Z, double A, 
	uInt8 *CommandExecuted);
/*!
 * LS4SetShowProt
 */
void __cdecl LS4SetShowProt(uInt8 *ShowProt, uInt8 *CommandExecuted);
/*!
 * LS4SetSnapshot
 */
void __cdecl LS4SetSnapshot(uInt8 *Snapshot, uInt8 *CommandExecuted);
/*!
 * LS4SetSnapshotPar
 */
void __cdecl LS4SetSnapshotPar(uInt8 *High, uInt8 *Automode, 
	uInt8 *CommandExecuted);
/*!
 * LS4SetSpeedPoti
 */
void __cdecl LS4SetSpeedPoti(uInt8 *SpeedPoti, 
	uInt8 *CommandExecuted);
/*!
 * LS4SetSwitchActive
 */
void __cdecl LS4SetSwitchActive(uInt32 XA, uInt32 YA, uInt32 ZA, 
	uInt32 AA, uInt8 *CommandExecuted);
/*!
 * LS4SetSwitchPolarity
 */
void __cdecl LS4SetSwitchPolarity(uInt32 XP, uInt32 YP, uInt32 ZP, 
	uInt32 AP, uInt8 *CommandExecuted);
/*!
 * LS4SetTargetWindow
 */
void __cdecl LS4SetTargetWindow(double X, double Y, double Z, double A, 
	uInt8 *CommandExecuted);
/*!
 * LS4SetTrigger
 */
void __cdecl LS4SetTrigger(uInt8 *Trigger, uInt8 *CommandExecuted);
/*!
 * LS4SetTriggerPar
 */
void __cdecl LS4SetTriggerPar(uInt32 Axis, uInt32 Mode, uInt32 Signal, 
	double Distance, uInt8 *CommandExecuted);
/*!
 * LS4SetTVRMode
 */
void __cdecl LS4SetTVRMode(int32 XT, int32 YT, int32 ZT, int32 AT, 
	uInt8 *CommandExecuted);
/*!
 * LS4SetVel
 */
void __cdecl LS4SetVel(double X, double Y, double Z, double A, 
	uInt8 *CommandExecuted);
/*!
 * LS4SetVelSingleAxis
 */
void __cdecl LS4SetVelSingleAxis(int32 Axis, double Vel, 
	uInt8 *CommandExecuted);
/*!
 * LS4SetWriteLogText
 */
void __cdecl LS4SetWriteLogText(uInt8 *WriteLogText, 
	uInt8 *CommandExecuted);
/*!
 * LS4SetWriteLogTextFN
 */
void __cdecl LS4SetWriteLogTextFN(uInt8 *WriteLogText, 
	char LogFilename[], uInt8 *CommandExecuted);
/*!
 * Enables/disables the X-Y axis compensation mode (see LStep documentation)
 */
void __cdecl LS4SetXYAxisComp(uInt32 Value, uInt8 *CommandExecuted);
/*!
 * LS4SoftwareReset
 */
void __cdecl LS4SoftwareReset(uInt8 *CommandExecuted);
/*!
 * LS4StopAxes
 */
void __cdecl LS4StopAxes(uInt8 *CommandExecuted);
/*!
 * The VI returns when selected axes have reached the final position 
 * 
 * LS_WaitForAxisStop uses ‚?statusaxis' to poll the status of the axes. The 
 * function returns when the axes selected in the bit mask Aflags have 
 * stopped.
 */
void __stdcall LS4WaitForAxisStop(uInt8 *X, uInt8 *Y, uInt8 *Z, 
	uInt8 *A, int32 TimeoutValue, uInt8 *CommandExecuted, 
	uInt8 *Timeout);

long __cdecl LVDLLStatus(char *errStr, int errStrLen, void *module);

#ifdef __cplusplus
} // extern "C"
#endif

#pragma pack(pop)

