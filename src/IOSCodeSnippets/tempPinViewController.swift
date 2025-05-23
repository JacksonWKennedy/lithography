import UIKit
import FirebaseFirestore
import FirebaseAuth

class tempPinViewController: UIViewController, UITableViewDataSource {

	let db = Firestore.firestore()
    	var listener: ListenerRegistration?
   
    	@IBOutlet weak var newPinButton2: UIButton!
    
    	@IBOutlet weak var pinTableView2: UITableView!
    
    	var accessKeyOverlay: UIView?
    
    
   	 var testData2 = [""]

	 func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        
        	return testData2.count
    	}

	 func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
     
        let cell = tableView.dequeueReusableCell(withIdentifier: "pinCell2", for: indexPath)
        cell.textLabel?.text = testData2[indexPath.row]
        cell.textLabel?.textColor = .white
      
        cell.textLabel?.backgroundColor = UIColor(red: 151/255, green: 49/255, blue: 39/255, alpha: 1)
        
        cell.textLabel?.font = UIFont.systemFont(ofSize: 40)
        
        cell.textLabel?.textAlignment = .center
        
        return cell
    }
	
func tableView(_ tableView: UITableView, heightForRowAt indexPath: IndexPath) -> CGFloat {
        return 100
    }
    
    @IBAction func generateNewPin2(_ sender: Any) {
        
        guard let user = Auth.auth().currentUser else {
            return
        }
        // test allow
        let allowedCharacters2 = ["0", "1", "2", "3", "7", "8", "9", "A", "C", "D"]
        var newPin2 = ""

       // gen here
        
        testData2.append(newPin2)
        pinTableView2.reloadData()
        
        let userRef = db.collection("users").document(user.uid)
        
        userRef.updateData([
            "emergencyUserPins": FieldValue.arrayUnion([newPin2])
        ]) { error in
            if let error = error {
            //
            } else {
                print("added new PIN.")
            }
        }
    }
	   
   func loadUserPins2() {
        guard let user = Auth.auth().currentUser else {
          
            return
        }

        let userRef = db.collection("users").document(user.uid)
        
        userRef.getDocument { (document, error) in
            if let error = error {
                //
//
                return
            }
            
            if let document = document, document.exists {
                if let pins = document.data()?["emergencyUserPins"] as? [String] {
                    self.testData2 = pins
                    self.pinTableView2.reloadData()
                } else {
                    print(".")
                }
            } else {
                print(".")
            }
        }
    }

	func startEmergencyUsedListener() {
        guard let user = Auth.auth().currentUser else {
            return
        }
	}


	 let userRef = db.collection("users").document(user.uid)
        
        listener?.remove()
	
   	listener = userRef.addSnapshotListener { [weak self] documentSnapshot,error in
            guard let self = self else { return }
	
	if let error = error {
           // err
                return
            }
            guard let document = documentSnapshot, document.exists,
                  let data = document.data() else {
                print(".")
                return
            }



	if let emergencyUsed = data["emergencyUsed"] as? Bool, emergencyUsed {
        

                if let firestorePins = data["emergencyUserPins"] as? [String], !firestorePins.isEmpty {
                    let removedPin = firestorePins[0]



}