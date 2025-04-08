function showNotification(message, success = true) {
    const notif = document.getElementById("notif");
    notif.textContent = message;
    notif.style.color = success ? "green" : "red";
    notif.classList.remove("hidden");
    setTimeout(() => notif.classList.add("hidden"), 3000);
}

// Fungsi untuk mengirim Fingerprint ID ke MQTT
document.getElementById("enroll").addEventListener("click", function() {
    const fingerprint_id = document.getElementById("fingerprint_id").value;

    if (fingerprint_id < 1 || fingerprint_id > 127) {
        showNotification("Fingerprint ID harus antara 1 - 127!", false);
        return;
    }

    fetch("/enroll", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ fingerprint_id })
    })
    .then(response => response.json())
    .then(data => showNotification(data.message))
    .catch(error => showNotification("Error mengirim ke MQTT!", false));
});

// Fungsi untuk menyimpan data ke database
document.getElementById("daftar").addEventListener("click", function() {
    const nama = document.getElementById("nama").value;
    const fingerprint_id = document.getElementById("fingerprint_id").value;

    if (!nama || fingerprint_id < 1 || fingerprint_id > 127) {
        showNotification("Harap isi nama dan Fingerprint ID (1 - 127)!", false);
        return;
    }

    fetch("/register", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ nama, fingerprint_id })
    })
    .then(response => response.json())
    .then(data => showNotification(data.message))
    .catch(error => showNotification("Error menyimpan ke database!", false));
});
