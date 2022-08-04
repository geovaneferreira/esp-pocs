/* ***********
 * Project   : Esp-App-Mobile-iOS - App to connect a Esp32 device by BLE
 * Programmer: Joao Lopes
 * Module    : MainController - main controller and main code in app
 * Comments  : Uses a singleton pattern to share instance to all app
 * Versions  :
 * -------  --------    -------------------------
 * 0.1.0    08/08/18    First version
 * 0.1.1    17.08.18    Adjusts in send repeat echoes
 * 0.1.2    17/08/18    Adjusts in Terminal BLE and Debug
 * 0.2.0    20/08/18    Option to disable logging br BLE (used during repeated sends)
 * 0.3.0    23/08/18    Changed name of github repos to Esp-App-Mobile-Apps-*
 *                      Few adjustments
 * 0.3.1    24/08/18    Alert for BLE device low battery
 **/

/*
 // TODO:
 */

import Foundation
import UIKit

public class MainController: NSObject, BLEDelegate {
    /////// Singleton
    
    static private var instance : MainController {
        return sharedInstance
    }
    
    private static let sharedInstance = MainController()
    
    // Singleton pattern method
    
    static func getInstance() -> MainController {
        return self.instance
    }
    
    ////// BLE instance
    private let ble = BLE.getInstance ()

    
    ////// Objects
    
    var navigationController: UINavigationController? = nil // Navegation
    
    var storyBoardMain: UIStoryboard? = nil // Story board Main
    
    @objc var timerSeconds: Timer? = nil // Timer in seconds
    
    var imageBattery: UIImage? = nil // Battery
    
    ///// Variables
    
    public let versionApp:String = "0.3.1" // Version of this APP
    
    private (set) var versionDevice: String = "?" // Version of BLE device
    
    private (set) var timeFeedback: Int = 0 // Time to send feedbacks
    private (set) var sendFeedback: Bool = false // Send feedbacks ?
        
    private (set) var timeActive: Int = 0 // Time of last activity
    private var exiting: Bool = false
    
    private (set) var deviceHaveBattery: Bool = false       // Device connected have a battery
    private (set) var deviceHaveSenCharging: Bool = false   // Device connected have a sensor of charging of battery
    private (set) var poweredExternal: Bool = false         // Powered by external (USB or power supply)?
    private (set) var chargingBattery: Bool = false         // Charging battery ?
    private (set) var statusBattery:String = "100%"         // Battery status
    private (set) var voltageBattery: Float = 0.0           // Voltage calculated of battery
    private (set) var readADCBattery: Int = 0               // Value of ADC read of battery
    
    private (set) var debugging: Bool = true // Debugging ?
    
    // Global exception treatment // TODO: make it in future
    // private let mExceptionHandler: ExceptionHandler
        
    // BLE
            
    private var bleStatusActive: Bool = false       // Active BLE status in the panel?
    
    private var bleTimeout: Int = 0                 // BLE Timeout
    private var bleVerifyTimeout: Bool = true       // Check timeout?
    
    private var bleAbortingConnection: Bool = false // Aborting connection ?
    
    private (set) var bleDebugs: [BLEDebug] = []    // Array for BLE debugs
    public var bleDebugEnabled: Bool =
                    AppSettings.TERMINAL_BLE        // It is enabled ?
    
    /////////////////// Init
    
    override init() {
        super.init()
        // Initialize App
        initializeApp ()
        
    }
    
    private func initializeApp () {
        
        // Initialize the app
        // Debug - set level
        debugSetLevel(.verbose)
        debugV("Initilializing ...")
                
        // BLE
        ble.delegate = self                              // See it! please left only of 2 below lines uncommented
        //ble.showDebug(.debug)       // Less debug of BLE, only essential
        ble.showDebug(.verbose)   // More debug
        
        // Inicializa variaveis
        initializeVariables()
        debugV("Initialized")
        
    }
    
    public func initializeVariables () {
        // Initialize control variables
        bleVerifyTimeout = false
        bleTimeout = 0
        sendFeedback = false
        timeActive = AppSettings.TIME_MAX_INACTITIVY
        
    }
    
    func bleDidUpdateState(_ state: BLECentralManagerState) {
        if state == BLECentralManagerState.poweredOn {
            let message = "Bluetooth is enable"
            debugW(message)
            // Search devive
            bleScanDevice()
        } else {
            let message = "Bluetooth is disabled"
            debugW(message)
        }
    }
    
    func bleDidTimeoutScan() {
        let message = "Could not connect to device"
        debugV(" ", message)
    }
    
    func bleDidConnectingToPeripheral(_ name: String) {
        let message = "Found: \(name)"
        debugV("conection 1: ", message)
        
        if let mainVC:ViewController = navigationController?.topViewController as? ViewController {
            mainVC.lblStatus.text = message
            mainVC.lblStatus.textColor = UIColor.orange;
        }
    }
    
    func bleDidConnectToPeripheral(_ name: String) {
        let message = "Connected a \(name)"
        debugV("conection 2: ", message)
        
        if let mainVC:ViewController = navigationController?.topViewController as? ViewController {
            mainVC.lblStatus.text = message
            mainVC.lblStatus.textColor = UIColor.green;
        }
        
    }
    
    func bleDidReceiveData(data: String) {
        //none
    }
    
    func bleDidReceiveLine(line: String) {
        if let mainVC:ViewController = self.navigationController?.topViewController as? ViewController {
            let dateFormatter = DateFormatter()
            dateFormatter.dateFormat = "dd/MM/yyyy HH:mm:ss"
            let date = Date()
        
            
            //mainVC.txtPrompt.text = dateFormatter.string(from: date) + " " + line + "\n" + mainVC.txtPrompt.text;
            mainVC.txtPrompt.text = mainVC.txtPrompt.text + dateFormatter.string(from: date) + " " + line + "\n";
            let range = NSRange(location: mainVC.txtPrompt.text.count - 1, length: 0)
            mainVC.txtPrompt.scrollRangeToVisible(range)
        }
        
        // Multithreading
        DispatchQueue.global().async {
            debugV("msg: ", line)
        }
    }
    
    func bleDidDisconnectFromPeripheral() {
        let message = "Device disconnected (code B1)"
        debugV(message)
        bleAbortConnection(message: message) // Abort
       
        if let mainVC:ViewController = navigationController?.topViewController as? ViewController {
            mainVC.lblStatus.text = message
            mainVC.lblStatus.textColor = UIColor.red;
        }
        
    }
    
    ////// BLE
    public func bleScanDevice() {
        
        // Starting the scanning ...
        debugV("Scanning device ...")
       
        ble.startScan(AppSettings.BLE_DEVICE_NAME)
        
        if let mainVC:ViewController = navigationController?.topViewController as? ViewController {
            mainVC.lblStatus.text = "Scanning device ..."
            mainVC.lblStatus.textColor = UIColor.blue;
        }
    }
    
    // Abort the connection

    public func bleAbortConnection(message:String) {
        
        // Only if not still running, to avoid loops
        if bleAbortingConnection {
            return
        }
        // Abort
        bleAbortingConnection = true
        
        debugV("msg=" + message)
                    
        // Message to device enter in standby or reinitialize
        // Abort timers
        ble.disconnectPeripheral()
        // Init variables
        initializeVariables()
        
        bleAbortingConnection = false
    }
    
    func bleSendMessage(_ message:String ) {
        if !ble.connected || ble.sendingNow {
            return
        }
        
        DispatchQueue.global().async {
            // Debug
            debugV("send menssage \(message)")
            // Reinitializes the time for feedbacks
            self.timeFeedback = 0
            // Send a message by BLE
            self.ble.send(message)
        }
    }
   
}

////// END
