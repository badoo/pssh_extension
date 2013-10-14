<?php

$servers = array("62.109.17.46", "www.bsdway.ru", "www.bsdway.ru", "www.bsdway.ru");
$user = "vagner";
$public_key = "/home/vagner/.ssh/bsdway/bsdway.ru.pub";
$private_key = "/home/vagner/.ssh/bsdway/bsdway.ru";
$remote_file = "/tmp/bsdway.ru.pub";
$local_file = "/home/vagner/.ssh/bsdway/bsdway.ru.pub";

$r = pssh_init($user, $public_key, $private_key);

foreach ($servers as $server) {
	pssh_server_add($r, $server);
}

/* connect to the servers */
do { 
	$ret = pssh_connect($r, $server, 3); 
	switch ($ret) {
		case PSSH_CONNECTED:
			echo "connected to ", $server, "\n";
			unset($servers[array_search($server, $servers)]);
			break;
	}
} while ($ret == PSSH_CONNECTED);

if ($ret == PSSH_SUCCESS) {
	echo "all servers connected\n";
} else {
	echo "failed to connect to: ", implode(", ", $servers), "\n";
}

/* create a task */
$tl = pssh_tasklist_init($r);
pssh_copy_to_server($tl, $server, $local_file, $remote_file);

/* execute the task */
do { } while(pssh_tasklist_exec($tl, $server, 10) == PSSH_RUNNING);

/* output a report */
$failed = $succeeded = 0;
for ($t = pssh_tasklist_first($tl); $t; $t = pssh_tasklist_next($tl)) {
	if (pssh_task_status($t) == PSSH_TASK_DONE) {
		$succeeded++;
	} else {
		$failed++;
	}
}

echo "Commands succeeded: ".$succeeded."\n";
echo "Commands failed: ".$failed."\n";

?>
