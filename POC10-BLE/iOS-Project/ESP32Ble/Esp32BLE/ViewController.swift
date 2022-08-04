//
//  ViewController.swift
//  Esp32BLE
//
//  Created by Geovane Ferreira on 03/08/22.
//

import UIKit
import Foundation

class ViewController: UIViewController  {
    
    private let mainController = MainController.getInstance()
    
    @IBOutlet weak var lblStatus: UILabel!
    @IBOutlet weak var txtPrompt: UITextView!
    @IBOutlet weak var txtField: UITextField!
    
    override func viewDidLoad() {
        super.viewDidLoad()
        // Do any additional setup after loading the view.
        
        debugV("")
        mainController.navigationController = self.navigationController
    }

    override func didReceiveMemoryWarning() {
        super.didReceiveMemoryWarning()
        debugV("")
    }
    
    
    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        debugV("")
    }
    
    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        debugV("")
    }
    
    @IBAction func sendDataBLE(_ sender: Any) {
        mainController.bleSendMessage(txtField.text!)
    }
    
    @IBAction func reload(_ sender: Any) {
        mainController.bleAbortConnection(message: "reconnecting...");
        mainController.bleScanDevice();
    }
    
    
}

