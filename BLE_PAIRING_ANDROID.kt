/**
 * Android BLE Párosítási Rendszer
 * AutoEd fejegység párosítási logika
 * 
 * Kotlin/Java implementáció
 */

import android.bluetooth.*
import android.content.Context
import android.util.Log
import java.util.*

class BLEPairingManager(private val context: Context) {
    
    companion object {
        private const val TAG = "BLEPairing"
        
        // UUIDs
        val SERVICE_UUID: UUID = UUID.fromString("4fafc201-1fb5-459e-8fcc-c5c9c331914b")
        val CHAR_UUID_PIN: UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a8")
        val CHAR_UUID_STATUS: UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a9")
        
        const val DEVICE_NAME = "CarHead"
    }
    
    private val bluetoothManager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    private val bluetoothAdapter = bluetoothManager.adapter
    private var bluetoothGatt: BluetoothGatt? = null
    private var currentPin: String? = null
    private var pairingCallback: PairingCallback? = null
    
    // ============ PÁROSÍTÁSI CALLBACK INTERFÉSZ ============
    
    interface PairingCallback {
        fun onPairingStarted()
        fun onPinReceived(pin: String)
        fun onPairingSuccess()
        fun onPairingFailed(error: String)
        fun onStatusChanged(status: String)
    }
    
    fun setPairingCallback(callback: PairingCallback) {
        this.pairingCallback = callback
    }
    
    // ============ KERESÉS ============
    
    fun startScan(callback: (device: BluetoothDevice) -> Unit) {
        val scanner = bluetoothAdapter.bluetoothLeScanner
        
        val scanCallback = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult?) {
                super.onScanResult(callbackType, result)
                result?.let {
                    if (it.device.name == DEVICE_NAME) {
                        Log.d(TAG, "Eszköz találva: ${it.device.address}")
                        callback(it.device)
                    }
                }
            }
        }
        
        scanner.startScan(scanCallback)
        Log.d(TAG, "Keresés elindult")
    }
    
    // ============ CSATLAKOZÁS ============
    
    fun connectToDevice(device: BluetoothDevice) {
        Log.d(TAG, "Csatlakozás: ${device.address}")
        
        bluetoothGatt = device.connectGatt(context, false, gattCallback)
        pairingCallback?.onPairingStarted()
    }
    
    // ============ GATT CALLBACK ============
    
    private val gattCallback = object : BluetoothGattCallback() {
        
        override fun onConnectionStateChange(gatt: BluetoothGatt?, status: Int, newState: Int) {
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    Log.d(TAG, "GATT csatlakozva")
                    // kis késleltetés, majd szolgáltatások keresése
                    Thread.sleep(500)
                    bluetoothGatt?.discoverServices()
                }
                
                BluetoothProfile.STATE_DISCONNECTED -> {
                    Log.d(TAG, "GATT szétkapcsolódva")
                    pairingCallback?.onPairingFailed("Szétkapcsolódva az eszközről")
                }
            }
        }
        
        override fun onServicesDiscovered(gatt: BluetoothGatt?, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                Log.d(TAG, "Szolgáltatások felderítve")
                
                // PIN karakterisztika keresése
                val service = gatt?.getService(SERVICE_UUID)
                val pinCharacteristic = service?.getCharacteristic(CHAR_UUID_PIN)
                
                if (pinCharacteristic != null) {
                    Log.d(TAG, "PIN karakterisztika megtalálva")
                    // PIN lekérése az eszközről
                    readPin(gatt, pinCharacteristic)
                } else {
                    pairingCallback?.onPairingFailed("PIN karakterisztika nem található")
                }
            }
        }
        
        override fun onCharacteristicRead(
            gatt: BluetoothGatt?,
            characteristic: BluetoothGattCharacteristic?,
            status: Int
        ) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                when (characteristic?.uuid) {
                    CHAR_UUID_PIN -> {
                        val pin = String(characteristic.value)
                        Log.d(TAG, "PIN fogadva: $pin")
                        currentPin = pin
                        pairingCallback?.onPinReceived(pin)
                        
                        // Státusz feliratkozás
                        subscribeToStatus(gatt)
                    }
                }
            }
        }
        
        override fun onCharacteristicWrite(
            gatt: BluetoothGatt?,
            characteristic: BluetoothGattCharacteristic?,
            status: Int
        ) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                Log.d(TAG, "PIN írás sikeres")
                // Válasz vár a státusz karakterisztikán
            }
        }
        
        override fun onCharacteristicChanged(
            gatt: BluetoothGatt?,
            characteristic: BluetoothGattCharacteristic?
        ) {
            when (characteristic?.uuid) {
                CHAR_UUID_STATUS -> {
                    val status = String(characteristic.value)
                    Log.d(TAG, "Státusz: $status")
                    pairingCallback?.onStatusChanged(status)
                    
                    when (status) {
                        "PAIRED" -> {
                            Log.d(TAG, "Párosítás sikeres!")
                            pairingCallback?.onPairingSuccess()
                        }
                        "INVALID_PIN" -> {
                            Log.d(TAG, "PIN hibás!")
                            pairingCallback?.onPairingFailed("A PIN kód hibás")
                        }
                    }
                }
            }
        }
        
        override fun onDescriptorWrite(
            gatt: BluetoothGatt?,
            descriptor: BluetoothGattDescriptor?,
            status: Int
        ) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                Log.d(TAG, "Descriptor írás sikeres - Értesítések engedélyezve")
            }
        }
    }
    
    // ============ PIN KARAKTERISZTIKA MŰVELETEK ============
    
    private fun readPin(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
        gatt.readCharacteristic(characteristic)
    }
    
    fun writePinToDevice(pin: String) {
        bluetoothGatt?.let { gatt ->
            val service = gatt.getService(SERVICE_UUID)
            val characteristic = service?.getCharacteristic(CHAR_UUID_PIN)
            
            if (characteristic != null) {
                characteristic.value = pin.toByteArray()
                gatt.writeCharacteristic(characteristic)
                Log.d(TAG, "PIN írva az eszközre: $pin")
            }
        }
    }
    
    // ============ STÁTUSZ FELIRATKOZÁS ============
    
    private fun subscribeToStatus(gatt: BluetoothGatt) {
        val service = gatt.getService(SERVICE_UUID)
        val statusCharacteristic = service?.getCharacteristic(CHAR_UUID_STATUS)
        
        if (statusCharacteristic != null) {
            // Értesítés engedélyezése
            gatt.setCharacteristicNotification(statusCharacteristic, true)
            
            // CCCD descriptor írása
            val descriptor = statusCharacteristic.getDescriptor(
                UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
            )
            if (descriptor != null) {
                descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                gatt.writeDescriptor(descriptor)
                Log.d(TAG, "Státusz értesítésre feliratkozva")
            }
        }
    }
    
    // ============ TISZTÍTÁS ============
    
    fun disconnect() {
        bluetoothGatt?.disconnect()
        bluetoothGatt?.close()
        bluetoothGatt = null
        Log.d(TAG, "Szétkapcsolódás")
    }
}

// ============ FELHASZNÁLÁSI MINTA (MainActivity) ============

/*
class MainActivity : AppCompatActivity() {
    
    private lateinit var pairingManager: BLEPairingManager
    
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        
        pairingManager = BLEPairingManager(this)
        setupUI()
    }
    
    private fun setupUI() {
        // Keresés gomb
        findViewById<Button>(R.id.btnSearch).setOnClickListener {
            pairingManager.startScan { device ->
                pairingManager.connectToDevice(device)
            }
        }
        
        // Párosítási callback
        pairingManager.setPairingCallback(object : BLEPairingManager.PairingCallback {
            override fun onPairingStarted() {
                Log.d("UI", "Párosítás elindult...")
                updateUI("Csatlakozás az eszközhöz...")
            }
            
            override fun onPinReceived(pin: String) {
                Log.d("UI", "PIN megkapva: $pin")
                updateUI("PIN: $pin - Kérlek, írd be az alkalmazásba a bekötést!")
                
                // PIN beviteli dialog megjelenítése
                showPinInputDialog { enteredPin ->
                    if (enteredPin == pin) {
                        pairingManager.writePinToDevice(pin)
                    } else {
                        updateUI("Hibás PIN!")
                    }
                }
            }
            
            override fun onPairingSuccess() {
                Log.d("UI", "Párosítás sikeres!")
                updateUI("✓ Párosítás sikeres! Az eszköz mentve.")
            }
            
            override fun onPairingFailed(error: String) {
                Log.d("UI", "Párosítás hiba: $error")
                updateUI("✗ Hiba: $error")
            }
            
            override fun onStatusChanged(status: String) {
                Log.d("UI", "Státusz: $status")
            }
        })
    }
    
    private fun updateUI(message: String) {
        runOnUiThread {
            findViewById<TextView>(R.id.tvStatus).text = message
        }
    }
    
    private fun showPinInputDialog(callback: (pin: String) -> Unit) {
        val input = EditText(this)
        input.hint = "6 jegyű PIN"
        input.inputType = InputType.TYPE_CLASS_NUMBER
        
        AlertDialog.Builder(this)
            .setTitle("Adja meg a PIN kódot")
            .setView(input)
            .setPositiveButton("OK") { _, _ ->
                callback(input.text.toString())
            }
            .setNegativeButton("Mégse") { dialog, _ ->
                dialog.cancel()
            }
            .show()
    }
}
*/
