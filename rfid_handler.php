<?php
header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST');
header('Access-Control-Allow-Headers: Content-Type');

// Database credentials
$servername = "localhost";
$username = "root";
$password = "";
$dbname = "it414_db_TEAM_GO";


try {
    $pdo = new PDO("mysql:host=$servername;dbname=$dbname;charset=utf8mb4", $username, $password);
    $pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

    // Get RFID data from POST or GET
    $rfid_data = '';
    if ($_SERVER['REQUEST_METHOD'] === 'GET' && isset($_GET['rfid_data'])) {
        $rfid_data = trim($_GET['rfid_data']);
    } elseif ($_SERVER['REQUEST_METHOD'] === 'POST') {
        $input = json_decode(file_get_contents('php://input'), true);
        if ($input && isset($input['rfid_data'])) {
            $rfid_data = trim($input['rfid_data']);
        }
    }

    if (empty($rfid_data)) {
        echo json_encode(['error' => 'No RFID data provided']);
        exit;
    }

    // Check if RFID exists
    $stmt = $pdo->prepare("SELECT rfid_data, rfid_status FROM rfid_reg WHERE rfid_data = ?");
    $stmt->execute([$rfid_data]);
    $result = $stmt->fetch(PDO::FETCH_ASSOC);

    $current_time = date('Y-m-d H:i:s');

    if ($result) {
        // Toggle status
        $current_status = (int)$result['rfid_status'];
        $new_status = $current_status ? 0 : 1;

        // Update status in rfid_reg
        $update_stmt = $pdo->prepare("UPDATE rfid_reg SET rfid_status = ? WHERE rfid_data = ?");
        $update_stmt->execute([$new_status, $rfid_data]);

        // Log into rfid_logs
        $log_stmt = $pdo->prepare("INSERT INTO rfid_logs (time_log, rfid_data, rfid_status) VALUES (?, ?, ?)");
        $log_stmt->execute([$current_time, $rfid_data, $new_status]);

        // Publish to MQTT
        publishToMQTT($new_status);

        // Send JSON response
        echo json_encode([
            'found' => true,
            'rfid_data' => $rfid_data,
            'current_status' => $current_status,
            'status' => $new_status,
            'message' => 'RFID found and status updated',
            'time_logged' => $current_time
        ]);

    } else {
        // RFID not found, log as 0
        $log_stmt = $pdo->prepare("INSERT INTO rfid_logs (time_log, rfid_data, rfid_status) VALUES (?, ?, ?)");
        $log_stmt->execute([$current_time, $rfid_data, 0]);

        // Publish 0 to MQTT
        publishToMQTT(0);

        echo json_encode([
            'found' => false,
            'rfid_data' => $rfid_data,
            'status' => 0,
            'message' => 'RFID NOT FOUND',
            'time_logged' => $current_time
        ]);
    }

} catch (PDOException $e) {
    echo json_encode(['error' => 'Database error: ' . $e->getMessage()]);
}

// MQTT publish function
function publishToMQTT($status) {
    $mqtt_host = "localhost";
    $mqtt_port = 1883;
    $mqtt_topic = "RFID_LOGIN";

    $message = $status ? "1" : "0";

    // Windows-friendly mosquitto_pub command
    $command = "mosquitto_pub -h $mqtt_host -p $mqtt_port -t $mqtt_topic -m \"$message\"";

    exec($command, $output, $return_var);

    if ($return_var !== 0) {
        error_log("MQTT publish failed: $return_var");
    }
}
?>
