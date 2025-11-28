<?php
header('Content-Type: application/json');
header('Access-Control-Allow-Origin: *');

$servername = "localhost";
$username = "root";
$password = "";
$dbname = "it414_db_TEAM_GO";

try {
    $pdo = new PDO("mysql:host=$servername;dbname=$dbname;charset=utf8mb4", $username, $password);
    $pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
    
    $input = json_decode(file_get_contents('php://input'), true);
    $rfid_data = $input['rfid_data'];
    $new_status = $input['new_status'];
    
    $update_stmt = $pdo->prepare("UPDATE rfid_reg SET rfid_status = ? WHERE rfid_data = ?");
    $update_stmt->execute([$new_status, $rfid_data]);
    
    $current_time = date('Y-m-d H:i:s');
    $log_stmt = $pdo->prepare("INSERT INTO rfid_logs (time_log, rfid_data, rfid_status) VALUES (?, ?, ?)");
    $log_stmt->execute([$current_time, $rfid_data, $new_status]);
    
    // Publish to MQTT
    publishToMQTT($new_status);
    
    echo json_encode([
        'success' => true,
        'message' => 'Status updated successfully'
    ]);
    
} catch (PDOException $e) {
    echo json_encode([
        'success' => false,
        'error' => $e->getMessage()
    ]);
}

function publishToMQTT($status) {
    $mqtt_host = "localhost";
    $mqtt_port = 1883;
    $mqtt_topic = "RFID_LOGIN";
    
    $message = (string)$status;
    
    $command = sprintf(
        'mosquitto_pub -h %s -p %d -t %s -m "%s"',
        escapeshellarg($mqtt_host),
        $mqtt_port,
        escapeshellarg($mqtt_topic),
        escapeshellarg($message)
    );
    
    exec($command);
}
?>