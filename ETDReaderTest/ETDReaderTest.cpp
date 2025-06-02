// ETDReaderTest.cpp : Defines the entry point for the console application.
//
//	2/6/2025 - Copyright Lasse Lauwerys
//

#include "stdafx.h"


#define ETD_PROBE_DEVICE_COMMAND ((DWORD)0x4)
#define ETD_D100_COMMAND ((DWORD)263)
#define ETD_D101_COMMAND ((DWORD)519)
#define ETD_D102_COMMAND ((DWORD)775)
#define ETD_D103_COMMAND ((DWORD)1031)
#define ETD_D104_COMMAND ((DWORD)9223)
#define ETD_FCS_COMMAND ((DWORD)11783) // D105
#define ETD_TOUCHPAD_DIMENSIONS_COMMAND ((DWORD)776)
#define ETD_NEXT_PACKET_NUMBER_1_COMMAND ((DWORD)1032)
#define ETD_NEXT_PACKET_NUMBER_2_COMMAND ((DWORD)1544)
#define ETD_DEVICE_INFO_COMMAND ((DWORD)1287)
#define ETD_KERNEL_DATA_COMMAND ((DWORD)1288)
#define ETD_UNKNOWN_COMMAND_2 ((DWORD)3335)
#define ETD_UNKNOWN_COMMAND_3 ((DWORD)774) // Does it return data? It's just sent as a command.
#define ETD_COLLECT_PACKETS_COMMAND ((DWORD)1800)
#define ETD_TOUCHPAD_SETTINGS_COMMAND ((DWORD)4359)
#define ETD_TOUCHPAD_PROPERTIES_COMMAND ((DWORD)6151)
#define ETD_TOUCHPAD_DEBUG_INFO_1_COMMAND ((DWORD)1543)
#define ETD_BOOT_COMMAND_PROCESS_INFO_COMMAND ((DWORD)11527)
#define ETD_RECORD_PROCESS_INFO_COMMAND ((DWORD)11271)

#define ETD_CONTROL_CODE ((DWORD)0x9C412000)
#define ETD_BUFFER_SIZE ((DWORD)124814)

#pragma pack(push, 1)


typedef struct ETDDebugInfoPacket { // 26 dwords
	DWORD a;
	DWORD b;
	DWORD c;
	DWORD d;
	DWORD hResult;
	DWORD f;
	DWORD g;
	DWORD result;
	DWORD unknown[18];
};

typedef struct ETDDebugInfo { // 998528 bits
	WORD unknown1;
	ETDDebugInfoPacket packets[2400]; // 26 * 2400
	WORD unknown2;
	DWORD debugLength;
	DWORD deviceMode;
	DWORD powerSuspendTimes;
};

typedef struct ETDCommand {
	BYTE firstNumber;
	BYTE secondNumber;
};

typedef struct ETDDeviceInfoe {
	ETDCommand command;
	BYTE unk1;
	BYTE kbcMode; // if 0, "Legacy", else "Multiplex"
	DWORD touchpadPort;
	DWORD protocolVersion; // Stuff for each touchpad, their index is the port.
};



typedef struct ETDInputPacket { // 9 words
	DWORD empty1;
	BYTE fingerCountOrSo;
	DWORD empty2;
	DWORD packetCount;
	DWORD index;
	BYTE value;
};

typedef struct ETDInputData {
	ETDCommand command;
	WORD packetId;
	WORD unknown1;
	DWORD packetNumber;
	DWORD packetCount;
	ETDInputPacket packets[1024];
	WORD nextPacketId;
};
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct ETDEmptiness {
	DWORD gap[257];
};

typedef struct ETDTouchpadSize {
	ETDCommand version;
	ETDEmptiness gap;
	DWORD empty;
	DWORD touchpadWidth;
	DWORD touchpadHeight;
};
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct ETDD {
	ETDCommand version;
	ETDEmptiness gap;
	DWORD empty;
	DWORD a, b, c;
};

typedef struct ETDNextPacketData {
	ETDCommand version;
	ETDEmptiness gap;
	DWORD nextPacketId;
};

typedef struct FingerPosition {
	COORD position;
	COORD size;
	BYTE pressure;
};

typedef struct FingerData {
	FingerPosition positions[5];
};
#pragma pack(pop)

class ETDReader {
public:
	ETDReader();
	~ETDReader();

	LPWORD GetBuffer();
	template <typename T>
	T *GetBuffer();
	BOOL GetDeviceData(DWORD command, DWORD pointer = 0);
	template <typename T>
	T GetDeviceData(DWORD command, DWORD pointer = 0);
	BOOL GetTouchpadSize(int *width, int *height);
	BOOL WaitForInput();
	ETDInputData *GetTouchData();
	ETDD GetD100();
	ETDD GetD101();
	ETDD GetD102();
	ETDD GetD103();
	ETDD GetD104();
	ETDD GetFCS();

	VOID GetFingerPositions(FingerData *fingerPositions, INT *fingerPositionCount, BOOL *isTrackPoint = NULL);

	VOID GetNextPacketId() {
		GetDeviceData(ETD_NEXT_PACKET_NUMBER_2_COMMAND);

		//protocolVersion = Is6PacketProtocol();

		ETDNextPacketData *data = GetBuffer<ETDNextPacketData>();
		nextPacketId = data->nextPacketId;
	}

	BOOL IsClickPad() {
		ETDD d101 = GetD101();
		return d101.b & 0x10;
	}

	BOOL IsSmartPad() {
		return !IsClickPad();
	}

	BOOL IsImageSensorDevice() {
		ETDD d101 = GetD101();
		return (d101.a & 0xFu) - 2 > 3;
	}

	BOOL IsProfileDevice() {
		return !IsImageSensorDevice();
	}

	BYTE GetButtons() {
		ETDInputPacket packet = GetTouchData()->packets[IsImageSensorDevice() ? 1 : 0];
		return packet.value & 0xF;
	}

	BOOL IsLeftMouseDown() {
		BYTE buttonValue = GetButtons();
		return buttonValue & 0x1;
	}

	BOOL IsRightMouseDown() {
		BYTE buttonValue = GetButtons();
		return buttonValue & 0x2;
	}

	BOOL IsMiddleClickDown() {
		BYTE buttonValue = GetButtons();
		return buttonValue & 0x4;
	}

	/*BYTE GetFingerCount() {
		ETDInputPacket packet = GetTouchData()->packets[0];
		BYTE fingerCount = (packet.value & 0xC0) >> 6;
		return fingerCount;
	}*/

	ETDDeviceInfoe GetDeviceInfo() {
		return this->GetDeviceData<ETDDeviceInfoe>(ETD_DEVICE_INFO_COMMAND);
	}

	//BOOL Is6PacketProtocol() {
	//	ETDDeviceInfoe info = GetDeviceInfo();
	//	DWORD value = info.protocolVersion;
	//	// 0x00F2DD7C
	//	// 0x00F2DD84
	//	BOOL is = value >= 0x2000;
	//	return is;
	//}

	INT GetProtocolVersion() {
		return protocolVersion;
	}

private:
	HANDLE deviceHandle;
	WORD buffer[62408];
	HANDLE eventHandle;
	WORD nextPacketId;
	INT protocolVersion = 0;
};

ETDReader::ETDReader() {
	this->deviceHandle = CreateFileA("\\\\.\\ETD", 0xC0000000, 0, 0, 3, 0, 0);
	this->eventHandle = OpenEventA(0x1F0003u, 0, "Global\\ETDOther_GetKernelData");

	//GetDeviceData(ETD_PROBE_DEVICE_COMMAND);

	ETDDeviceInfoe info = GetDeviceInfo();
	protocolVersion = info.protocolVersion;

	GetNextPacketId();
}

ETDReader::~ETDReader() {
	CloseHandle(this->eventHandle);
	CloseHandle(this->deviceHandle);
	VirtualFree(this->buffer, 0, MEM_RELEASE);
}

ETDInputData *ETDReader::GetTouchData() {
	return (ETDInputData *)this->buffer;
}

BOOL ETDReader::GetDeviceData(DWORD command, DWORD pointer) {
	DWORD bytesReturned;
	this->buffer[0] = command;
	*(DWORD *)&this->buffer[1] = pointer;
	return DeviceIoControl(deviceHandle, ETD_CONTROL_CODE, this->buffer, ETD_BUFFER_SIZE, this->buffer, ETD_BUFFER_SIZE, &bytesReturned, 0);
}

LPWORD ETDReader::GetBuffer() {
	return this->buffer;
}

template<typename T>
T *ETDReader::GetBuffer() {
	return (T*)this->buffer;
}

BOOL ETDReader::GetTouchpadSize(int *width, int *height) {
	BOOL result = this->GetDeviceData(ETD_TOUCHPAD_DIMENSIONS_COMMAND);

	ETDTouchpadSize *touchpadInfo = (ETDTouchpadSize *)this->buffer;

	*width = touchpadInfo->touchpadWidth;
	*height = touchpadInfo->touchpadHeight;
	return result;
}

BOOL ETDReader::WaitForInput() {
	WaitForSingleObject(this->eventHandle, 0xFFFFFFFF);
	return ResetEvent(this->eventHandle);
}

template <typename T>
T ETDReader::GetDeviceData(DWORD command, DWORD pointer) {
	if (this->GetDeviceData(command, pointer))
		return *(T *)this->buffer;
}

ETDD ETDReader::GetD100() {
	return this->GetDeviceData<ETDD>(ETD_D100_COMMAND);
}

ETDD ETDReader::GetD101() {
	return this->GetDeviceData<ETDD>(ETD_D101_COMMAND);
}

ETDD ETDReader::GetD102() {
	return this->GetDeviceData<ETDD>(ETD_D102_COMMAND);
}

ETDD ETDReader::GetD103() {
	return this->GetDeviceData<ETDD>(ETD_D103_COMMAND);
}

ETDD ETDReader::GetD104() {
	return this->GetDeviceData<ETDD>(ETD_D104_COMMAND);
}

ETDD ETDReader::GetFCS() {
	return this->GetDeviceData<ETDD>(ETD_FCS_COMMAND);
}

VOID ETDReader::GetFingerPositions(FingerData *fingerPositions, INT *fingerPositionCount, BOOL *isTrackPoint) {
	BOOL a = this->GetDeviceData(ETD_KERNEL_DATA_COMMAND, nextPacketId);
	ETDInputData *data = this->GetBuffer<ETDInputData>();
	nextPacketId = data->nextPacketId;

	BOOL isTrackPad = TRUE;

	BYTE version = data->command.secondNumber;
	BYTE totalPacketCount = data->packetCount;

	BYTE inputData = data->packets[0].value;
	BYTE packetCount = data->packets[0].packetCount;
	if (packetCount == 32) { // TouchPad input
			WORD fingerBitMask = data->packets[1].value;
			INT fingerCount = 0;
			WORD checkMask = 0x1;
			for (int i = 0; i < 8; ++i)
				if (fingerBitMask & (checkMask <<= 1))
					++fingerCount;

			*fingerPositionCount = fingerCount;

			INT positionCount = data->packets[0].packetCount / 6;

			//INT packetOffset = 1;
			for (INT i = 0; i < fingerCount; ++i) {
				INT offset = i * 5 + 2;
				BYTE fingerPositionMS = data->packets[offset].value;
				BYTE fingerXLS = data->packets[offset + 1].value;
				BYTE fingerYLS = data->packets[offset + 2].value;
				BYTE fingerSize = data->packets[offset + 3].value;
				BYTE fingerPressure = data->packets[offset + 4].value;

				fingerPositions->positions[i].position.X = ((fingerPositionMS & 0xF0) << 4) + fingerXLS;
				fingerPositions->positions[i].position.Y = ((fingerPositionMS & 0xF) << 8) + fingerYLS;
				fingerPositions->positions[i].size.X = fingerSize & 0xF;
				fingerPositions->positions[i].size.Y = (fingerSize & 0xF0) >> 4;
				fingerPositions->positions[i].pressure = fingerPressure;
			}
	}
	else if (packetCount == 7) { // TrackPoint input
		isTrackPad = FALSE;
		INT8 trackPointX = data->packets[5].value;
		INT8 trackPointY = data->packets[6].value;


		//FingerPosition finger = fingerPositions->positions[0];
		fingerPositions->positions[0].position.X = static_cast<INT>(trackPointX);
		fingerPositions->positions[0].position.Y = static_cast<INT>(trackPointY);

		// 0x00BEDBD4

		//printf("Got pack: %d, It'sTrackPoint %d, X: %d\tY: %d\t", trackPointX, isTrackPad, finger.position.X, finger.position.Y);

		*fingerPositionCount = 1;
	}
	else if (protocolVersion < 0x300) {  // Otherwise the older 6 packet protocol
		printf("Unimplemented protocol");
		/*
		packet count probably 6 per finger, probably only supports 2 finger positions
		FormatString(&touchDisplay0, "[%d,%d] [X] [X]", v288 | (16 * (v287 & 0x10u)), v289 | (8 * (v287 & 0x20u)));
		FormatString(&touchDisplay1, "[%d,%d] [X] [X]", v291 | (16 * (v290 & 0x10u)), v292 | (8 * (v290 & 0x20u)));
		*/
	}
	else if (protocolVersion >= 0x2000) {
		printf("Unimplemented protocol");
		/*
		packet count probably 6 per finger and 5 finger support
		"[%d,%d] [%d] [%d]"
		*/

	}
	else {
		// This implementation tested working for version 0x310
		//printf("default protoocl");

		INT positionCount = data->packets[0].packetCount / 6;
		*fingerPositionCount = positionCount;

		for (int i = 0; i < positionCount; ++i) {
			BYTE packetOffset = i * 6;

			BYTE fingerSMS = data->packets[packetOffset + 0].value;
			BYTE fingerXMS = data->packets[packetOffset + 1].value;
			BYTE fingerXLS = data->packets[packetOffset + 2].value;
			BYTE fingerSLS = data->packets[packetOffset + 3].value;
			BYTE fingerYMS = data->packets[packetOffset + 4].value;
			BYTE fingerYLS = data->packets[packetOffset + 5].value;

			BYTE touchCount = (fingerSMS & 0xC) >> 6;
			BYTE size = ((fingerSMS & 0x30) >> 2) + ((fingerSLS & 0x30) >> 4);
			FingerPosition finger = fingerPositions->positions[i];
			fingerPositions->positions[i].position.X = ((fingerXMS & 0x0F) << 8) + fingerXLS;
			fingerPositions->positions[i].position.Y = ((fingerYMS & 0x0F) << 8) + fingerYLS;
			fingerPositions->positions[i].pressure = (fingerXMS & 0xF0) + (fingerYMS >> 4);
			fingerPositions->positions[i].size.X = fingerPositions->positions[i].size.Y = size;
		}
	}

	if (*isTrackPoint) *isTrackPoint = !isTrackPad;
}

class Console {
public:
	Console() {
		hOut = CreateFileA("CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	}

	VOID HideCursor() {
		CONSOLE_CURSOR_INFO cursorInfo;
		GetConsoleCursorInfo(hConsole, &cursorInfo);
		cursorInfo.bVisible = FALSE;
		SetConsoleCursorInfo(hConsole, &cursorInfo);
	}

	VOID Clear() {
		CONSOLE_SCREEN_BUFFER_INFO bufferInfo;
		GetConsoleScreenBufferInfo(hConsole, &bufferInfo);
		DWORD consoleSize = bufferInfo.dwSize.X * bufferInfo.dwSize.Y;
		DWORD charsWritten;
		FillConsoleOutputCharacterA(hConsole, ' ', consoleSize, { 0, 0 }, &charsWritten);
	}

	VOID SetCursorPosition(INT x = 0, INT y = 0) {
		SetConsoleCursorPosition(hConsole, { x, y });
	}

	VOID Render() {
		WriteFile(hOut, outBuffer, outPosition, NULL, NULL);
		outPosition = 0;
	}

	template<class... Args>
	VOID Write(char *format, Args&&... args) {
		outPosition += sprintf_s(outBuffer + outPosition, sizeof(outBuffer) - outPosition, format, args...);
	}

	template<class... Args>
	VOID WriteLine(char *format = NULL, Args&&... args) {
		if (format) Write(format, args...);
		Write("\n");
	}

	VOID PrintBuffer(LPVOID buffer, size_t size, size_t offset = 0) {
		for (size_t i = offset; i < offset + size; ++i)
			Write("%02X", ((LPCH)buffer)[i]);
	}
private:
	HANDLE hOut;
	HANDLE hConsole;
	char outBuffer[8192];
	int outPosition = 0;
};


int _tmain(int argc, _TCHAR* argv[])
{
	Console *console = new Console();
	console->HideCursor();
	console->Clear();

	ETDReader *reader = new ETDReader();
	LPWORD buffer = reader->GetBuffer();

	ETDInputData *data = reader->GetTouchData();

	int width, height;
	reader->GetTouchpadSize(&width, &height);

	WORD lastValue;

	BOOL result;
	result = reader->GetDeviceData(ETD_PROBE_DEVICE_COMMAND);

	ETDD d100 = reader->GetD100();
	ETDD d101 = reader->GetD101();
	ETDD d102 = reader->GetD102();
	ETDD d103 = reader->GetD103();
	ETDD d104 = reader->GetD104();
	ETDD fcs = reader->GetFCS();

	char *supportedFingers = reader->IsImageSensorDevice() ?  "5" : "3";
	char *touchpadType = reader->IsClickPad() ? "ClickPad" : "SmartPad";

	FingerData fingerData;
	INT oldFingerCount = 0;
	INT fingerPositionCount;

	while (reader->WaitForInput()) {
		console->Write("Touchpad resolution %d %d\n", width, height);
		console->Write("Type: %s\tSupported fingers: %s\n", touchpadType, supportedFingers);

		BOOL isTrackPoint;
		reader->GetFingerPositions(&fingerData, &fingerPositionCount, &isTrackPoint);

		//BYTE fingerPositionCount = reader->GetFingerCount();

		if (fingerPositionCount != oldFingerCount) console->Clear();

		oldFingerCount = fingerPositionCount;

		console->Write("Finger count: %d\t\n", fingerPositionCount);

		if (!isTrackPoint) {
			for (int i = 0; i < fingerPositionCount; ++i) {
				FingerPosition finger = fingerData.positions[i];
				COORD position = finger.position;
				COORD size = finger.size;
				INT pressure = finger.pressure;
				console->Write("Finger %d: X: %d\tY: %d\tPressure: %d\tSize: %dx%d\t\n", i, position.X, position.Y, pressure, size.X, size.Y);
			}
		}
		else {
			FingerPosition finger = fingerData.positions[0];
			COORD position = finger.position;
			console->Write("TrackPoint: X: %d \tY: %d\t\n", position.X, position.Y);

		}

		BOOL rightClick = reader->IsRightMouseDown(), leftClick = reader->IsLeftMouseDown(), middleClick = reader->IsMiddleClickDown();
		console->WriteLine("Left click: %s\t Right click: %s\t Middle click: %s\t", leftClick ? "True" : "False", rightClick ? "True" : "False", middleClick ? "True" : "False");
		
		console->Write("D100: %.2X %.2X %.2X\t", d100.a, d100.b, d100.c);
		console->Write("D101: %.2X %.2X %.2X\t", d101.a, d101.b, d101.c);
		console->Write("D102: %.2X %.2X %.2X\t", d102.a, d102.b, d102.c);
		console->Write("D103: %.2X %.2X %.2X\t", d103.a, d103.b, d103.c);
		console->Write("D104: %.2X %.2X %.2X\t", d104.a, d104.b, d104.c);
		console->Write("FCS: %.2X %.2X %.2X\t", fcs.a, fcs.b, fcs.c);
		console->WriteLine();
		console->Write("Protocol version: 0x%.2X\t", reader->GetProtocolVersion());
		console->Write("Packets sent: %d\n", data->packetNumber);
		console->Write("Next packet number: %d\n", data->packetId);
		console->Write("Event packet count: %d \n", data->packetCount);
		console->Write("Header: ");
		console->PrintBuffer(buffer, 11);
		console->Write("\tFooter: ");
		console->PrintBuffer(&(data->nextPacketId), 32);
		console->WriteLine("\n");

		for (int i = 0; i < data->packetCount && i < 30; ++i) {
			ETDInputPacket packet = data->packets[i];
			console->Write("Packet index: %d \t Packet value: %02X\t%d\t%i\t\n", packet.index, packet.value, packet.value, packet.value);
		}

		console->Render();
		console->SetCursorPosition();
	}
	return 0;
}

void GetSettings() {
	char *name;
	name = "ButtonEnable";
	name = "Left_Button_Func";
	name = "Middle_Button_Func";
	name = "Right_Button_Func";
	name =  "ClickPad_LeftCorner_Click_Enable";
	name =  "ClickPad_LeftCorner_Click_Func";
	name =  "ClickPad_RightCorner_Click_Enable";
	name =  "ClickPad_RightCorner_Click_Func";
	name =  "ClickPad_Two_Finger_Click_Enable";
	name =  "ClickPad_Two_Finger_Click_Func";
	name =  "ClickPad_Three_Finger_Click_Enable";
	name =  "ClickPad_Three_Finger_Click_Func";
	name =  "ClickPad_Four_Finger_Click_Enable";
	name =  "ClickPad_Four_Finger_Click_Func";
	name = "Zoom_Enable";
	name = "Zoom_STV";
	name = "Rotation_Enable";
	name = "Rotation_Circular_ShowItem";
	name = "Rotation_Circular_Enable";
	name = "Drag_Enable";
	name = "Drag_Radio";
	name = "Drag_Radio2_Slider";
	name =  "Drag_Delay_Time_Adjust_Enable";
	name =  "Drag_Delay_Time_Adjust_Value";
	name = "Two_V_Scroll_Enable";
	name = "Two_H_Scroll_ShowItem";
	name = "Two_H_Scroll_Enable";
	name = "Two_Auto_Scroll_ShowItem";
	name = "Two_Auto_Scroll_Enable";
	name =  "SC_InertialScroll_Enable";
	name =  "SC_ContinueScroll_Enable";
	name =  "SC_Reverse_Enable";
	name = "Two_Scroll_Speed";
	name =  "SC_Free_Enable";
	name = "Tap_Enable";
	name = "Tap_T1_Time";
	name = "Tap1_Func";
	name = "Tap2_ShowItem";
	name =  "Tap2_Enable";
	name = "Tap2_Func";
	name = "Tap3_ShowItem";
	name = "Tap3_Func";
	name = "STV_Enable";
	name = "STV_Silder";
	name = "Palm_Enable";
	name = "Palm_Slider";
	name = "Swipe_Page_Enable";
	name = "Swipe_Page_Kind";
	name = "ThreeFingerMove_Up_Enable";
	name = "ThreeFingerMove_Up_Func";
	name = "ThreeFingerMove_Up_Mode";
	name = "ThreeFingerMove_Down_Enable";
	name = "ThreeFingerMove_Down_Func";
	name = "ThreeFingerMove_Down_Mode";
	name = "ThreeFingerMove_Left_Enable";
	name = "ThreeFingerMove_Left_Func";
	name = "ThreeFingerMove_Left_Mode";
	name = "ThreeFingerMove_Right_Enable";
	name = "ThreeFingerMove_Right_Func";
	name = "ThreeFingerMove_Right_Mode";
	name = "SM_Enable";
	name = "SM_Radio";
	name = "SM_Radio2_Slider";
	name = "SM_Up_Range";
	name = "SM_Down_Range";
	name = "SM_Left_Range";
	name = "SM_Right_Range";
	name = "SM_Triger_Counter";
	name = "SL_Enable";
	name = "SL_Key";
	name =  "Edge_V_Scroll_Enable";
	name =  "Edge_H_Scroll_Enable";
	name =  "Edge_Auto_Scroll_Enable";
	name =  "EGS_InertialScroll_Enable";
	name =  "EGS_ContinueScroll_Enable";
	name =  "EGS_Reverse_Enable";
	name =  "Edge_V_Scroll_Area";
	name =  "Edge_V_Scroll_Area";
	name =  "Edge_Scroll_Speed";
	name =  "EGS_CornerArea_Reject_Enable";
	name =  "Circular_Scroll_ShowItem";
	name =  "Circular_Scroll_Enable";
	name =  "DoubleTapToOnOffDevice_ShowItem";
	name =  "DoubleTapToOnOffDevice_Enable";
	name =  "Mag_Enable";
	name =  "Mag_Kind";
	name =  "Momentum_Enable";
	name =  "Momentum_Slider";
	name =  "Momentum_Bounce_Enable";
	name =  "SmartArea_Enable";
	name =  "SmartArea_Up_Range";
	name =  "SmartArea_Down_Range";
	name =  "SmartArea_Left_Range";
	name =  "SmartArea_Right_Range";
	name =  "FourFingerMove_Up_Enable";
	name =  "FourFingerMove_Up_Func";
	name =  "FourFingerMove_Down_Enable";
	name =  "FourFingerMove_Down_Func";
	name =  "FourFingerMove_Left_Enable";
	name =  "FourFingerMove_Left_Func";
	name =  "FourFingerMove_Right_Enable";
	name =  "FourFingerMove_Right_Func";
	name =  "Palm_Always_Enable";
	name =  "EdgeZoom_Enable";
	name =  "Edge_1F_SwipePage_Enable";
	name =  "Edge_2F_SwipePage_Enable";
	name =  "Tap3_Press_Enable";
	name =  "Cover_Enable";
	name =  "Cover_Func";
	name =  "Corner_Tap_Enable";
	name =  "Corner_Tap_BL_Func";
	name =  "Corner_Tap_BL_Width";
	name =  "Corner_Tap_BL_Height";
	name =  "Corner_Tap_BR_Func";
	name =  "Corner_Tap_BR_Width";
	name =  "Corner_Tap_BR_Height";
	name =  "Corner_Tap_TL_Func";
	name =  "Corner_Tap_TL_Width";
	name =  "Corner_Tap_TL_Height";
	name =  "Corner_Tap_TR_Func";
	name =  "Corner_Tap_TR_Width";
	name =  "Corner_Tap_TR_Height";
	name =  "TwoFinger_Double_Tap_ShowItem";
	name =  "TwoFinger_Double_Tap_Enable";
	name =  "DualMode_TP_Mode";
	name =  "DisableWhenType_Enable";
	name =  "DisableWhenType_AllArea";
	name =  "DisableWhenType_DelayTime_Tap";
	name =  "DisableWhenType_DelayTime_Move";
	name =  "DisableWhenType_DelayTime_Gesture";
	name =  "DynamicPalmTap_Enable";
	name =  "DynamicPalmTap_RatioX";
	name =  "DynamicPalmTap_RatioY";
	name =  "Win8EdgeSwipeLeft_Enable";
	name =  "Win8EdgeSwipeRight_Enable";
	name =  "Win8EdgeSwipeTop_Enable";
	name =  "Win8EdgeSwipeBottom_Enable";
	name =  "Grab_Enable";
	name =  "Scroll_2F_Enable";
	name =  "Scroll_Edge_Enable";
	name =  "Swipe_3F_Enable";
	name =  "Swipe_4F_Enable";
	name =  "Swipe_4F_Enable_Mode";
	name =  "SmartDetect_Enable";
	name =  "SmartDetect_Up_Range";
	name =  "SmartDetect_Down_Range";
	name =  "SmartDetect_Left_Range";
	name =  "SmartDetect_Right_Range";
	name =  "Tap_Activate_Enable";
	name =  "Tap1_Enable";
	name =  "Tap3_Enable";
	name =  "Gesture_Effect_Mode";
	name =  "Win8_Edge_Swipe_Advance_Enable";
	name =  "Win8_Edge_Swipe_Advance_Left_Return_Middle_Enable";
	name =  "Win8_Edge_Swipe_Advance_Left_Return_End_Enable";
	name =  "Win8_Edge_Swipe_Advance_Left_To_Down_Enable";
	name =  "Win8_Edge_Swipe_Advance_Up_To_Down_Enable";
	name =  "Win8_Edge_Swipe_Advance_Left_Enable";
	name =  "Win8_Edge_Swipe_Advance_Top_Enable";
	name =  "Win8_Edge_Swipe_Advance_Right_Enable";
	name =  "Win8_Edge_Swipe_Advance_Bottom_Enable";
	name =  "DWT_Unit";
	name =  "DWT_Left_Range";
	name =  "DWT_Right_Range";
	name =  "DWT_Top_Range";
	name =  "DWT_Bottom_Range";
	name =  "DWT_AZone_Touch_Then_Free";
	name =  "Scroll_2F_Enable_ShowItem";
	name =  "Two_V_Scroll_ShowItem";
	name =  "Win8_Edge_Swipe_Edge_Range";
	name =  "Win8_Edge_Swipe_Triger_Radius";
	name =  "Win8_Edge_Swipe_Triger_Angle";
	name =  "Win8_Edge_Swipe_Mode";
	name =  "FreeTyping_Enable";
	name =  "FreeTyping_Mode";
	name =  "SmartArea_Restrict_Move";
	name =  "SmartArea_Restrict_Tap";
	name =  "SmartArea_IdleTime";
	name =  "SmartArea_IdleTime_Enable";
	name =  "SC_Speed_Has_8_Level";
	name =  "EGS_Speed_Has_8_Level";
	name =  "SmartArea_IdleTime_Unit";
	name =  "MultiFingerGesture_Enable";
	name =  "SmartArea_Unit";
	name =  "SmartArea_BreakTime";
	name =  "Win8EdgeSwipe_CharmSelect_Enable";
	name =  "Disable_When_Type_By_AP_Time";
}

void GetTouchPadProperties() {
	char *name;
	name = "TapEnable";
	name = "MKValueEnable";
	name = "MKValueMoveEnable";
	name = "DisableWhenType_Tap";
	name = "DisableWhenType_Move";
	name = "DisableWhenType_Gesture";
	name = "DisableWhenUSBMouse";
	name = "ELANTouchPadPort";
	name = "ELANPointStickPort";
	name = "PreInputButtons_LB";
	name = "PreInputButtons_MB";
	name = "PreInputButtons_RB";
	name = "MasterDeviceMode";
	name = "PowerChangeETDDisable";
	name = "OneTimeTap";
	name = "Dragflag";
	name = "DragLockFlag";
	name = "TapOutRange";
	name = "TapT1TimeTimerEnable";
	name = "TapT1TimeFlag";
	name = "NowGesture";
	name = "SM_Not_In_Area";
	name = "SL_Key_Down";
	name = "EdgeScroll_Triger";
	name = "EdgeScroll_OutArea";
	name = "ThreeFingerMove_Pre_Status";
	name = "TrayIcon_NowLevel";
	name = "SwipePage_Pre_Status";
	name =  "CanWriteCommand";
	name =  "ToolMode";
	name =  "ToolMode_Intercept";
	name =  "TouchPadUIDisplay_Not_Use_INF";
	name =  "FingerQuadrant";
	name =  "AP_TWO_FINGER_SCROLL";
	name =  "AP_ZOOM";
	name =  "AP_ROTATE";
	name =  "AP_ZOOM_ONE_TIME";
	name =  "AP_ROTATE_FREE";
	name =  "AP_SCROLLOBJECT_V_POWER";
	name =  "AP_SCROLLOBJECT_V_UNIT";
	name =  "AP_SCROLLOBJECT_H_POWER";
	name =  "AP_SCROLLOBJECT_H_UNIT";
	name =  "AP_SCROLL_V_TO_APIX";
	name =  "AP_ENABLE_TOUCHINJECTION_SCROLL";
	name =  "AP_ENABLE_TOUCHINJECTION_ZOOM";
	name =  "AP_ENABLE_TOUCHINJECTION_ROTATE";
	name =  "AutoScrollFunctionDisable";
	name =  "Tap_STV_OK";
	name =  "OpMode";
	name =  "CanDoGesture";
	name =  "PreGesture";
	name =  "EnableQueryMultiplexing";
	name =  "ScreenOrientation";
	name =  "bDisableTouchpadByLidClose";
	name =  "AtLoginScreenStatus";
	name =  "LidSwitchStateStatus";
	name =  "AP_SCROLL_H_TO_KERNEL";
	name =  "DiscardPacket";
	name =  "SupportCRCCheck";
	name =  "MultiFinger_Palm_Support";
	name =  "SMBus_Support_Self_dv";
	name =  "SMBus_Support_PS2_MultiPalm";
	name =  "AP_ZOOM_FREE";
	name =  "bIsPowerNoiseSolutionVerG";
	name =  "LED_Support";
	name =  "FiveButtonDualMode";
	name =  "SupportSmartPalm";
	name =  "PacketMouseDataLength";
	name =  "PacketNumber";
	name =  "bMergeCheckEnable";
	name =  "bMergeOccurs";
	name =  "SmartArea.RestrictMove";
	name =  "MomentumFunctionStart";
	name =  "SmartArea.RestrictTap";
	name =  "AutoScrollEnable";
	name =  "PSTWorkAndDisableTouchPadEnable";
	name =  "PSTWorkAndDisableTouchPad";
	name =  "AllocateWorkItemFailCount";
}

void GetDebugInfo(ETDReader *reader) {
	int v148;
	char *v149;

	ETDDebugInfo debugInfo = reader->GetDeviceData<ETDDebugInfo>(ETD_RECORD_PROCESS_INFO_COMMAND);

	switch (debugInfo.packets[0].result)
	{
	case 0x10u:
		v148 = 24i64;
		v149 = " [SMBus] [Get Object OK]";
		break;
	case 0x11u:
		v148 = 26i64;
		v149 = " [SMBus] [Get Object Fail]";
		break;
	case 0x12u:
		v148 = 36i64;
		v149 = " [SMBus] [Set Host Configuration OK]";
		break;
	case 0x13u:
		v148 = 38i64;
		v149 = " [SMBus] [Set Host Configuration Fail]";
		break;
	case 0x14u:
		v148 = 34i64;
		v149 = " [SMBus] [Set Block Read Write OK]";
		break;
	case 0x15u:
		v148 = 36i64;
		v149 = " [SMBus] [Set Block Read Write Fail]";
		break;
	case 0x16u:
		v148 = 30i64;
		v149 = " [SMBus] [Get Hello Packet OK]";
		break;
	case 0x17u:
		v148 = 32i64;
		v149 = " [SMBus] [Get Hello Packet Fail]";
		break;
	case 0x18u:
		v148 = 29i64;
		v149 = " [SMBus] [Enable TouchPad OK]";
		break;
	case 0x19u:
		v148 = 31i64;
		v149 = " [SMBus] [Enable TouchPad Fail]";
		break;
	case 0x1Au:
		v148 = 30i64;
		v149 = " [SMBus] [Disable TouchPad OK]";
		break;
	case 0x1Bu:
		v148 = 32i64;
		v149 = " [SMBus] [Disable TouchPad Fail]";
		break;
	case 0x1Cu:
		v148 = 27i64;
		v149 = " [SMBus] [Create Thread OK]";
		break;
	case 0x1Du:
		v148 = 29i64;
		v149 = " [SMBus] [Create Thread Fail]";
		break;
	case 0x1Eu:
		v148 = 29i64;
		v149 = " [SMBus] [Set Host Notify OK]";
		break;
	case 0x1Fu:
		v148 = 31i64;
		v149 = " [SMBus] [Set Host Notify Fail]";
		break;
	case 0x20u:
		v148 = 25i64;
		v149 = " [SMBus] [Query INUSE OK]";
		break;
	case 0x21u:
		v148 = 27i64;
		v149 = " [SMBus] [Query INUSE Fail]";
		break;
	case 0x100u:
		v148 = 15i64;
		v149 = " [SMBus] [None]";
		break;
	case 0x101u:
		v148 = 32i64;
		v149 = " [SMBus] [Device Object Is Null]";
		break;
	case 0x102u:
		v148 = 26i64;
		v149 = " [SMBus] [Read Write Fail]";
		break;
	case 0x103u:
		v148 = 29i64;
		v149 = " [SMBus] [Read Write Timeout]";
		break;
	case 0x104u:
		v148 = 29i64;
		v149 = " [SMBus] [Transfer Data Fail]";
		break;
	case 0x105u:
		v148 = 23i64;
		v149 = " [SMBus] [Start Device]";
		break;
	case 0x106u:
		v148 = 24i64;
		v149 = " [SMBus] [Remove Device]";
		break;
	case 0x107u:
		v148 = 22i64;
		v149 = " [SMBus] [Occur Retry]";
		break;
	case 0x108u:
		v148 = 22i64;
		v149 = " [SMBus] [Occur Abort]";
		break;
	default:
		printf(" [SMBus] [0x%.2X]", debugInfo.packets[0].hResult);
		//v149 = v303;
		//v148 = *((unsigned int *)v303 - 4);
		break;

	}
}

	/*switch (debugInfo.packets[0].result)
	{
	case 1u:
		v148 = 19i64;
		v130 = " [Response Timeout]";
		break;
	case 0x10u:
		v129 = 24i64;
		v130 = " [Tool Command Response]";
		break;
	case 0x11u:
		v129 = 32i64;
		v130 = " [Tool Command Response Timeout]";
		break;
	}
}*/