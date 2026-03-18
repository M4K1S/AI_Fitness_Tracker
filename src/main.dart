import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart';
import 'package:permission_handler/permission_handler.dart';

void main() {
  runApp(const AIFitnessTrackerApp());
}

class AIFitnessTrackerApp extends StatelessWidget {
  const AIFitnessTrackerApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'AI Fitness Tracker',
      theme: ThemeData(primarySwatch: Colors.blue),
      home: const BleAutoConnectScreen(),
    );
  }
}

class BleAutoConnectScreen extends StatefulWidget {
  const BleAutoConnectScreen({super.key});

  @override
  State<BleAutoConnectScreen> createState() => _BleAutoConnectScreenState();
}

class _BleAutoConnectScreenState extends State<BleAutoConnectScreen> {
  final FlutterReactiveBle _ble = FlutterReactiveBle();
  final List<DiscoveredDevice> _devices = [];
  final String _deviceNameFilter = "SquatTracker";

  bool _isScanning = false;
  bool _connected = false;

  StreamSubscription<DiscoveredDevice>? _scanStream;
  StreamSubscription<ConnectionStateUpdate>? _connection;
  StreamSubscription<List<int>>? _repSubscription;
  QualifiedCharacteristic? _repCharacteristic;

  @override
  void initState() {
    super.initState();
    _startScan();
  }

  Future<void> _requestPermissions() async {
    await [
      Permission.bluetooth,
      Permission.bluetoothScan,
      Permission.bluetoothConnect,
      Permission.location,
    ].request();
  }

  void _startScan() async {
    _devices.clear();
    setState(() => _isScanning = true);

    await _requestPermissions();

    _scanStream = _ble.scanForDevices(withServices: []).listen((device) {
      if (device.name == _deviceNameFilter &&
          !_devices.any((d) => d.id == device.id)) {
        setState(() {
          _devices.add(device);
        });

        // Auto-connect as soon as we see it
        if (!_connected) {
          _connectToDevice(device.id);
        }
      }
    }, onError: (error) {
      debugPrint("Scan error: $error");
      setState(() => _isScanning = false);
    });

    Future.delayed(const Duration(seconds: 10), _stopScan);
  }

  void _stopScan() {
    _scanStream?.cancel();
    setState(() => _isScanning = false);
  }

  void _connectToDevice(String deviceId) {
    _connection?.cancel();

    _connection = _ble.connectToDevice(id: deviceId).listen((event) {
      setState(() => _connected = event.connectionState == DeviceConnectionState.connected);

      if (_connected) {
        _repCharacteristic = QualifiedCharacteristic(
          serviceId: Uuid.parse("12345678-1234-1234-1234-1234567890ab"),
          characteristicId: Uuid.parse("abcd1234-5678-90ab-cdef-1234567890ab"),
          deviceId: deviceId,
        );

        _repSubscription = _ble
            .subscribeToCharacteristic(_repCharacteristic!)
            .listen((data) {
          // nothing here, handled in live screen
        });

        Navigator.push(
          context,
          MaterialPageRoute(
            builder: (context) => LiveRepScreen(
              repStream: _repSubscription!,
              deviceName: _deviceNameFilter,
            ),
          ),
        );

        _stopScan();
      }
    }, onError: (error) {
      debugPrint("Connection error: $error");
      setState(() => _connected = false);
    });
  }

  @override
  void dispose() {
    _scanStream?.cancel();
    _connection?.cancel();
    _repSubscription?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Connect Your Squat Tracker'),
        actions: [
          IconButton(
            icon: Icon(_isScanning ? Icons.stop : Icons.search),
            onPressed: _isScanning ? _stopScan : _startScan,
          )
        ],
      ),
      body: Column(
        children: [
          Expanded(
            child: _devices.isEmpty
                ? Center(
                    child: _isScanning
                        ? const CircularProgressIndicator()
                        : const Text('No SquatTracker found'),
                  )
                : ListView.builder(
                    itemCount: _devices.length,
                    itemBuilder: (context, index) {
                      final device = _devices[index];
                      return ListTile(
                        title: Text(device.name),
                        subtitle: Text(device.id),
                        trailing: _connected
                            ? const Icon(Icons.check, color: Colors.green)
                            : const Icon(Icons.bluetooth),
                      );
                    },
                  ),
          ),
        ],
      ),
    );
  }
}

// ---------------- Live Rep Screen ----------------

class LiveRepScreen extends StatefulWidget {
  final StreamSubscription<List<int>> repStream;
  final String deviceName;

  const LiveRepScreen({
    super.key,
    required this.repStream,
    required this.deviceName,
  });

  @override
  State<LiveRepScreen> createState() => _LiveRepScreenState();
}

class _LiveRepScreenState extends State<LiveRepScreen> {
  String _displayText = "";

  @override
  void initState() {
    super.initState();
    widget.repStream.onData((data) {
      final raw = String.fromCharCodes(data).trim();

      // Match your ESP32 format: "REP 1 | PHASE: 2"
      final match = RegExp(r'REP\s*(\d+)\s*\|\s*PHASE:\s*(\d+)').firstMatch(raw);

      if (match != null) {
        final repNum = match.group(1);
        final phaseNum = match.group(2);

        String phaseName = "";
        switch (phaseNum) {
          case "0":
          case "4":
            phaseName = "At the Top";
            break;
          case "1":
            phaseName = "Going Down";
            break;
          case "2":
            phaseName = "In the Hole";
            break;
          case "3":
            phaseName = "Going Up";
            break;
          default:
            phaseName = phaseNum!;
        }

        setState(() {
          _displayText = "REP $repNum: PHASE: $phaseName";
        });
      } else {
        setState(() {
          _displayText = raw; // fallback in case format changes
        });
      }
    });
  }

  @override
  void dispose() {
    widget.repStream.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text('${widget.deviceName} Live Data'),
      ),
      body: Center(
        child: Text(
          _displayText.isNotEmpty ? _displayText : "Waiting for data...",
          style: const TextStyle(fontSize: 28, fontWeight: FontWeight.bold),
          textAlign: TextAlign.center,
        ),
      ),
    );
  }
}
