<?php
/* $Id$ */
/*
  Copyright (C) 2008 Seth Mos
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
  AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
  AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
  OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

  */

/* include all configuration functions */
require_once("functions.inc");
require_once("pkg-utils.inc");
require_once("notices.inc");
require_once("globals.inc");

function dump_rrd_to_xml($rrddatabase, $xmldumpfile) {
	$rrdtool = "/usr/bin/nice -n20 /usr/local/bin/rrdtool";
	if(file_exists($xmldumpfile))
		exec("rm {$xmldumpfile}");

	exec("$rrdtool dump {$rrddatabase} {$xmldumpfile} 2>&1", $dumpout, $dumpret);
	if ($dumpret <> 0) {
		$dumpout = implode(" ", $dumpout);
		log_error("RRD dump failed exited with $dumpret, the error is: $dumpout");
	}
	return($dumpret);
}

function create_new_rrd($rrdcreatecmd) {
	$rrdcreateoutput = array();
	$rrdcreatereturn = 0;

	exec("$rrdcreatecmd 2>&1", $rrdcreateoutput, $rrdcreatereturn);
	if ($rrdcreatereturn <> 0) {
		$rrdcreateoutput = implode(" ", $rrdcreateoutput);
		log_error("RRD create failed exited with $rrdcreatereturn, the error is: $rrdcreateoutput");
	}
	return $rrdcreatereturn;
}

function migrate_rrd_format($rrdoldxml, $rrdnewxml) {
	$numrraold = count($rrdoldxml['rra']);
	$numdsold = count($rrdoldxml['ds']);
	$numrranew = count($rrdnewxml['rra']);
	$numdsnew = count($rrdnewxml['ds']);
	log_error("Import RRD has $numdsold DS values and $numrraold RRA databases, new format RRD has $numdsnew DS values and $numrranew RRA databases");
	
	/* add data sources not found in the old array from the new array */
	$i = 0;
	foreach($rrdnewxml['ds'] as $ds) {
		if(!is_array($rrdoldxml['ds'][$i])) {
			$rrdoldxml['ds'][$i] = $rrdnewxml['ds'][$i];
			/* set unknown values to 0 */
			$rrdoldxml['ds'][$i]['last_ds'] = " 0.0000000000e+00 ";
			$rrdoldxml['ds'][$i]['value'] = " 0.0000000000e+00 ";
			$rrdoldxml['ds'][$i]['unknown_sec'] = "0";
		}
		$i++;
	}

	$i = 0;
	$rracountold = count($rrdoldxml['rra']);
	$rracountnew = count($rrdnewxml['rra']);
	/* process each RRA, which contain a database */
	foreach($rrdnewxml['rra'] as $rra) {
		if(!is_array($rrdoldxml['rra'][$i])) {
			$rrdoldxml['rra'][$i] = $rrdnewxml['rra'][$i];
		}

		$d = 0;
		/* process cdp_prep */
		$cdp_prep = $rra['cdp_prep'];
		foreach($cdp_prep['ds'] as $ds) {
			if(!is_array($rrdoldxml['rra'][$i]['cdp_prep']['ds'][$d])) {
				$rrdoldxml['rra'][$i]['cdp_prep']['ds'][$d] = $rrdnewxml['rra'][$i]['cdp_prep']['ds'][$d];
				$rrdoldxml['rra'][$i]['cdp_prep']['ds'][$d]['primary_value'] = " 0.0000000000e+00 ";
				$rrdoldxml['rra'][$i]['cdp_prep']['ds'][$d]['secondary_value'] = " 0.0000000000e+00 ";
				$rrdoldxml['rra'][$i]['cdp_prep']['ds'][$d]['value'] = " 0.0000000000e+00 ";
				$rrdoldxml['rra'][$i]['cdp_prep']['ds'][$d]['unknown_datapoints'] = "0";
			}
			$d++;
		}

		/* process database */
		$rows = $rra['database'];
		$k = 0;
		$rowcountold = count($rrdoldxml['rra'][$i]['database']['row']);
		$rowcountnew = count($rrdnewxml['rra'][$i]['database']['row']);
		$rowcountdiff = $rowcountnew - $rowcountold;
		/* save old rows for a bit before we put the required empty rows before it */
		$rowsdata = $rows;
		$rowsempty = array();
		$r = 0;
		while($r < $rowcountdiff) {
			$rowsempty[] = $rrdnewxml['rra'][$i]['database']['row'][$r];
			$r++;
		}
		$rows = $rowsempty + $rowsdata;
		/* now foreach the rows in the database */
		foreach($rows['row'] as $row) {
			if(!is_array($rrdoldxml['rra'][$i]['database']['row'][$k])) {
				$rrdoldxml['rra'][$i]['database']['row'][$k] = $rrdnewxml['rra'][$i]['database']['row'][$k];
			}
			$m = 0;
			$vcountold = count($rrdoldxml['rra'][$i]['database']['row'][$k]['v']);
			$vcountnew = count($rrdnewxml['rra'][$i]['database']['row'][$k]['v']);
			foreach($row['v'] as $value) {
				if(empty($rrdoldxml['rra'][$i]['database']['row'][$k]['v'][$m])) {
					if(isset($valid)) {
						$rrdoldxml['rra'][$i]['database']['row'][$k]['v'][$m] = "0.0000000000e+00 ";
					} else {
						$rrdoldxml['rra'][$i]['database']['row'][$k]['v'][$m] = $rrdnewxml['rra'][$i]['database']['row'][$k]['v'][$m];
					}
				} else {
					if($value <> " NaN ") {
						$valid = true;
					} else {
						$valid = false;
					}
				}
				$m++;
			}
			$k++;
		}
		$i++;
	}

	$numrranew = count($rrdoldxml['rra']);
	$numdsnew = count($rrdoldxml['ds']);
	log_error("The new RRD now has $numdsnew DS values and $numrranew RRA databases");
	return $rrdoldxml;
}

function enable_rrd_graphing() {
	global $config, $g, $altq_list_queues;

	if($g['booting']) 
		echo "Generating RRD graphs...";

	$rrddbpath = "/var/db/rrd/";
	$rrdgraphpath = "/usr/local/www/rrd";

	$traffic = "-traffic.rrd";
	$packets = "-packets.rrd";
	$states = "-states.rrd";
	$wireless = "-wireless.rrd";
	$queues = "-queues.rrd";
	$queuesdrop = "-queuedrops.rrd";
	$spamd = "-spamd.rrd";
	$proc = "-processor.rrd";
	$mem = "-memory.rrd";

	$rrdtool = "/usr/bin/nice -n20 /usr/local/bin/rrdtool";
	$netstat = "/usr/bin/netstat";
	$awk = "/usr/bin/awk";
	$tar = "/usr/bin/tar";
	$pfctl = "/sbin/pfctl";
	$sysctl = "/sbin/sysctl";
	$php = "/usr/local/bin/php";
	$top = "/usr/bin/top";
	$spamd_gather = "/usr/local/bin/spamd_gather_stats.php";
	$ifconfig = "/sbin/ifconfig";

	$rrdtrafficinterval = 60;
	$rrdwirelessinterval = 60;
	$rrdqueuesinterval = 60;
	$rrdqueuesdropinterval = 60;
	$rrdpacketsinterval = 60;
	$rrdstatesinterval = 60;
	$rrdspamdinterval = 60;
	$rrdlbpoolinterval = 60;
	$rrdprocinterval = 60;
	$rrdmeminterval = 60;

	$trafficvalid = $rrdtrafficinterval * 2;
	$wirelessvalid = $rrdwirelessinterval * 2;
	$queuesvalid = $rrdqueuesinterval * 2;
	$queuesdropvalid = $rrdqueuesdropinterval * 2;
	$packetsvalid = $rrdpacketsinterval * 2;
	$statesvalid = $rrdstatesinterval*2;
	$spamdvalid = $rrdspamdinterval * 2;
	$lbpoolvalid = $rrdlbpoolinterval * 2;
	$procvalid = $rrdlbpoolinterval * 2;
	$memvalid = $rrdmeminterval * 2;

	/* Asume GigE for now */
	$downstream = 125000000;
	$upstream = 125000000;

	/* read the shaper config */
	read_altq_config();

	$rrdrestore = "";
	$rrdreturn = "";

	if (isset ($config['rrd']['enable'])) {

		/* create directory if needed */
		if (!is_dir("$rrddbpath")) {
			mkdir("$rrddbpath", 0755);
		}

		if ($g['booting']) {
			if ($g['platform'] != "pfSense") {
				/* restore the databases, if we have one */
				if (file_exists("{$g['cf_conf_path']}/rrd.tgz")) {
					exec("cd /;LANG=C /usr/bin/tar -xzf {$g['cf_conf_path']}/rrd.tgz 2>&1", $rrdrestore, $rrdreturn);
					$rrdrestore = implode(" ", $rrdrestore);
					if($rrdreturn <> 0) {
						log_error("RRD restore failed exited with $rrdreturn, the error is: $rrdrestore\n");
					}
				}
			}
		}

		/* db update script */
		$rrdupdatesh = "#!/bin/sh\n";
		$rrdupdatesh .= "\n";
		$rrdupdatesh .= "counter=1\n";
		$rrdupdatesh .= "while [ \"\$counter\" -ne 0 ]\n";
		$rrdupdatesh .= "do\n";
		$rrdupdatesh .= "";

		$i = 0;
		$vfaces = array (
			"vlan.?*",
			"enc.?*"
		);
		$ifdescrs = get_interface_list(true, true, $vfaces);
		$ifdescrs['enc0']['friendly'] = "ipsec";
		$ifdescrs['enc0']['descr'] = "IPsec";
		$ifdescrs['enc0']['up'] = true;

		foreach ($ifdescrs as $realif => $ifdescr) {
			$ifname = $ifdescr['friendly'];
			$state = $ifdescr['up'];

			/* skip interfaces that do not have a friendly name */
			if ("$ifname" == "") {
				continue;
			}

			/* or are down */
			if (!$state) {
				continue;
			}

			/* TRAFFIC, set up the rrd file */
			if (!file_exists("$rrddbpath$ifname$traffic")) {
				$rrdcreate = "$rrdtool create $rrddbpath$ifname$traffic --step $rrdtrafficinterval ";
				$rrdcreate .= "DS:inpass:COUNTER:$trafficvalid:0:$downstream ";
				$rrdcreate .= "DS:outpass:COUNTER:$trafficvalid:0:$upstream ";
				$rrdcreate .= "DS:inblock:COUNTER:$trafficvalid:0:$downstream ";
				$rrdcreate .= "DS:outblock:COUNTER:$trafficvalid:0:$upstream ";
				$rrdcreate .= "RRA:AVERAGE:0.5:1:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:5:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:60:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:720:3000 ";

				create_new_rrd($rrdcreate);
			}

			/* enter UNKNOWN values in the RRD so it knows we rebooted. */
			if($g['booting']) {
				exec("$rrdtool update $rrddbpath$ifname$traffic N:U:U:U:U");
			}

			$rrdupdatesh .= "\n";
			$rrdupdatesh .= "# polling traffic for interface $ifname $realif \n";
			$rrdupdatesh .= "$rrdtool update $rrddbpath$ifname$traffic N:\\\n";
			$rrdupdatesh .= "`$pfctl -vvsI -i {$realif} | awk '/In4\/Pass|Out4\/Pass/ {printf \$6 \":\"}'`\\\n";
			$rrdupdatesh .= "`$pfctl -vvsI -i {$realif} | awk '/In4\/Block|Out4\/Block/ {printf \$6 \":\"}'|sed -e 's/.\$//'`\n";

			/* PACKETS, set up the rrd file */
			if (!file_exists("$rrddbpath$ifname$packets")) {
				$rrdcreate = "$rrdtool create $rrddbpath$ifname$packets --step $rrdpacketsinterval ";
				$rrdcreate .= "DS:inpass:COUNTER:$packetsvalid:0:$downstream ";
				$rrdcreate .= "DS:outpass:COUNTER:$packetsvalid:0:$upstream ";
				$rrdcreate .= "DS:inblock:COUNTER:$packetsvalid:0:$downstream ";
				$rrdcreate .= "DS:outblock:COUNTER:$packetsvalid:0:$upstream ";
				$rrdcreate .= "RRA:AVERAGE:0.5:1:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:5:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:60:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:720:3000 ";

				create_new_rrd($rrdcreate);
			}

			/* enter UNKNOWN values in the RRD so it knows we rebooted. */
			if($g['booting']) {
				exec("$rrdtool update $rrddbpath$ifname$packets N:U:U:U:U");
			}

			$rrdupdatesh .= "\n";
			$rrdupdatesh .= "# polling packets for interface $ifname $realif \n";
			$rrdupdatesh .= "$rrdtool update $rrddbpath$ifname$packets N:\\\n";
			$rrdupdatesh .= "`$pfctl -vvsI -i {$realif} | awk '/In4\/Pass|Out4\/Pass/ {printf \$4 \":\"}'`\\\n";
			$rrdupdatesh .= "`$pfctl -vvsI -i {$realif} | awk '/In4\/Block|Out4\/Block/ {printf \$4 \":\"}'|sed -e 's/.\$//'`\n";

			/* WIRELESS, set up the rrd file */
			if($config['interfaces'][$ifname]['wireless']['mode'] == "bss") {
				if (!file_exists("$rrddbpath$ifname$wireless")) {
					$rrdcreate = "$rrdtool create $rrddbpath$ifname$wireless --step $rrdwirelessinterval ";
					$rrdcreate .= "DS:snr:GAUGE:$wirelessvalid:0:1000 ";
					$rrdcreate .= "DS:rate:GAUGE:$wirelessvalid:0:1000 ";
					$rrdcreate .= "DS:channel:GAUGE:$wirelessvalid:0:1000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:1:1000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:5:1000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:60:1000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:720:3000 ";
	
					create_new_rrd($rrdcreate);
				}

				/* enter UNKNOWN values in the RRD so it knows we rebooted. */
				if($g['booting']) {
					exec("$rrdtool update $rrddbpath$ifname$wireless N:U:U:U");
				}

				$rrdupdatesh .= "\n";
				$rrdupdatesh .= "# polling wireless for interface $ifname $realif \n";
				$rrdupdatesh .= "$rrdtool update $rrddbpath$ifname$wireless N:\\\n";
				$rrdupdatesh .= "`$ifconfig {$realif} list ap| $awk 'gsub(\"M\", \"\") {getline 2;print substr(\$5, 0, length(\$5)-2) \":\" $4 \":\" $3}'`\n";
			}

				/* QUEUES, set up the queues databases */
				if ($altq_list_queues[$ifname]) {
					$altq =& $altq_list_queues[$ifname];
					/* NOTE: Is it worth as its own function?! */
					switch ($altq->GetBwscale()) {
                                        case "Gb":
                                        	$factor = 1024 * 1024 * 1024;
                                                break;
                                        case "Mb":
                                                $factor = 1024 * 1024;
                                                break;
                                        case "Kb":
                                                $factor = 1024;
                                                break;
                                        case "b":
                                        default:
                                                $factor = 1;
                                                break;
                                        }
					$qbandwidth = $altq->GetBandwidth() * $factor;
					if ($qbandwidth <=0)
						$qbandwidth = 100 * 1000 * 1000; /* 100Mbit */
					$qlist =& $altq->get_queue_list($notused);
					if (!file_exists("$rrddbpath$ifname$queues")) {
						$rrdcreate = "$rrdtool create $rrddbpath$ifname$queues --step $rrdqueuesinterval ";
						/* loop list of shaper queues */
						$q = 0;
						foreach ($qlist as $qname => $q) {
							$rrdcreate .= "DS:$qname:COUNTER:$queuesvalid:0:$qbandwidth ";
						}

						$rrdcreate .= "RRA:AVERAGE:0.5:1:1000 ";
						$rrdcreate .= "RRA:AVERAGE:0.5:5:1000 ";
						$rrdcreate .= "RRA:AVERAGE:0.5:60:1000 ";
						$rrdcreate .= "RRA:AVERAGE:0.5:720:3000 ";

						create_new_rrd($rrdcreate);
					}

					if (!file_exists("$rrddbpath$ifname$queuesdrop")) {
						$rrdcreate = "$rrdtool create $rrddbpath$ifname$queuesdrop --step $rrdqueuesdropinterval ";
						/* loop list of shaper queues */
						$q = 0;
						foreach ($qlist as $qname => $q) {
							$rrdcreate .= "DS:$qname:COUNTER:$queuesdropvalid:0:$qbandwidth ";
						}

						$rrdcreate .= "RRA:AVERAGE:0.5:1:1000 ";
						$rrdcreate .= "RRA:AVERAGE:0.5:5:1000 ";
						$rrdcreate .= "RRA:AVERAGE:0.5:60:1000 ";
						$rrdcreate .= "RRA:AVERAGE:0.5:720:3000 ";

						create_new_rrd($rrdcreate);
					}

					if($g['booting']) {
						$rrdqcommand = "-t ";
						$rrducommand = "N";
						$q = 0;
						foreach ($qlist as $qname => $q) {
							if($q == 0) {
								$rrdqcommand .= "{$qname}";
							} else {
								$rrdqcommand .= ":{$qname}";
							}
							$q++;
							$rrducommand .= ":U";
						}
						mwexec("$rrdtool update $rrddbpath$ifname$queues $rrdqcommand $rrducommand");
						mwexec("$rrdtool update $rrddbpath$ifname$queuesdrop $rrdqcommand $rrducommand");
					}

					/* awk function to gather shaper data */
					/* yes, it's special */
					$rrdupdatesh .= "` pfctl -vsq -i {$realif} | awk 'BEGIN {printf \"$rrdtool update $rrddbpath$ifname$queues \" } ";
					$rrdupdatesh .= "{ ";
					$rrdupdatesh .= "if ((\$1 == \"queue\") && ( \$2 ~ /^q/ )) { ";
					$rrdupdatesh .= "dsname = dsname \":\" \$2 ; ";
					$rrdupdatesh .= "q=1; ";
					$rrdupdatesh .= "} ";
					$rrdupdatesh .= "else if ((\$4 == \"bytes:\") && ( q == 1 ) ) { ";
					$rrdupdatesh .= "dsdata = dsdata \":\" \$5 ; ";
					$rrdupdatesh .= "q=0; ";
					$rrdupdatesh .= "} ";
					$rrdupdatesh .= "} END { ";
					$rrdupdatesh .= "dsname = substr(dsname,2); ";
					$rrdupdatesh .= "dsdata = substr(dsdata,2); ";
					$rrdupdatesh .= "printf \"-t \" dsname \" N:\" dsdata }' ";
					$rrdupdatesh .= "dsname=\"\" dsdata=\"\"`\n\n";

					$rrdupdatesh .= "` pfctl -vsq -i {$realif} | awk 'BEGIN {printf \"$rrdtool update $rrddbpath$ifname$queuesdrop \" } ";
					$rrdupdatesh .= "{ ";
					$rrdupdatesh .= "if ((\$1 == \"queue\") && ( \$2 ~ /^q/ )) { ";
					$rrdupdatesh .= "dsname = dsname \":\" \$2 ; ";
					$rrdupdatesh .= "q=1; ";
					$rrdupdatesh .= "} ";
					$rrdupdatesh .= "else if ((\$4 == \"bytes:\") && ( q == 1 ) ) { ";
					$rrdupdatesh .= "dsdata = dsdata \":\" \$8 ; ";
					$rrdupdatesh .= "q=0; ";
					$rrdupdatesh .= "} ";
					$rrdupdatesh .= "} END { ";
					$rrdupdatesh .= "dsname = substr(dsname,2); ";
					$rrdupdatesh .= "dsdata = substr(dsdata,2); ";
					$rrdupdatesh .= "printf \"-t \" dsname \" N:\" dsdata }' ";
					$rrdupdatesh .= "dsname=\"\" dsdata=\"\"`\n\n";
				}
		}
		$i++;

		/* System only statistics */
		$ifname = "system";

			/* STATES, create pf states database */
			if(! file_exists("$rrddbpath$ifname$states")) {
				$rrdcreate = "$rrdtool create $rrddbpath$ifname$states --step $rrdstatesinterval ";
				$rrdcreate .= "DS:pfrate:GAUGE:$statesvalid:0:10000000 ";
				$rrdcreate .= "DS:pfstates:GAUGE:$statesvalid:0:10000000 ";
				$rrdcreate .= "DS:pfnat:GAUGE:$statesvalid:0:10000000 ";
				$rrdcreate .= "DS:srcip:GAUGE:$statesvalid:0:10000000 ";
				$rrdcreate .= "DS:dstip:GAUGE:$statesvalid:0:10000000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:1:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:5:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:60:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:720:3000 ";

				create_new_rrd($rrdcreate);
			}

			/* enter UNKNOWN values in the RRD so it knows we rebooted. */
			if($g['booting']) {
				exec("$rrdtool update $rrddbpath$ifname$states N:U:U:U:U:U");
			}

 			/* the pf states gathering function. */
 			$rrdupdatesh .= "\n";
			$rrdupdatesh .= "pfctl_si_out=\"` $pfctl -si > /tmp/pfctl_si_out `\"\n";
			$rrdupdatesh .= "pfctl_ss_out=\"` $pfctl -ss > /tmp/pfctl_ss_out`\"\n";
			$rrdupdatesh .= "pfrate=\"` cat /tmp/pfctl_si_out | egrep \"inserts|removals\" | awk '{ pfrate = \$3 + pfrate } {print pfrate}'|tail -1 `\"\n";
 			$rrdupdatesh .= "pfstates=\"` cat /tmp/pfctl_ss_out | egrep -v \"<\\-.*?<\\-|\\->.*?\\->\" | wc -l|sed 's/ //g'`\"\n";
			$rrdupdatesh .= "pfnat=\"` cat /tmp/pfctl_ss_out | egrep '<\\-.*?<\\-|\\->.*?\\->' | wc -l|sed 's/ //g' `\"\n";
 			$rrdupdatesh .= "srcip=\"` cat /tmp/pfctl_ss_out | egrep -v '<\\-.*?<\\-|\\->.*?\\->' | grep '\\->' | awk '{print \$3}' | awk -F: '{print \$1}' | sort -u|wc -l|sed 's/ //g' `\"\n";
 			$rrdupdatesh .= "dstip=\"` cat /tmp/pfctl_ss_out | egrep -v '<\\-.*?<\\-|\\->.*?\\->' | grep '<\\-' | awk '{print \$3}' | awk -F: '{print \$1}' | sort -u|wc -l|sed 's/ //g' `\"\n";
			$rrdupdatesh .= "$rrdtool update $rrddbpath$ifname$states N:\$pfrate:\$pfstates:\$pfnat:\$srcip:\$dstip\n\n";

			/* End pf states statistics */

			/* CPU, create CPU statistics database */
			if(! file_exists("$rrddbpath$ifname$proc")) {
				$rrdcreate = "$rrdtool create $rrddbpath$ifname$proc --step $rrdprocinterval ";
				$rrdcreate .= "DS:user:GAUGE:$procvalid:0:10000000 ";
				$rrdcreate .= "DS:nice:GAUGE:$procvalid:0:10000000 ";
				$rrdcreate .= "DS:system:GAUGE:$procvalid:0:10000000 ";
				$rrdcreate .= "DS:interrupt:GAUGE:$procvalid:0:10000000 ";
				$rrdcreate .= "DS:processes:GAUGE:$procvalid:0:10000000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:1:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:5:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:60:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:720:3000 ";

				create_new_rrd($rrdcreate);
			}

			/* enter UNKNOWN values in the RRD so it knows we rebooted. */
			if($g['booting']) {
				exec("$rrdtool update $rrddbpath$ifname$proc N:U:U:U:U:U");
			}

 			/* the CPU stats gathering function. */
 			$rrdupdatesh .= "`$top -d 2 -s 1 0 | $awk '{gsub(/%/, \"\")} BEGIN { \\\n";
			$rrdupdatesh .= "printf \"$rrdtool update $rrddbpath$ifname$proc \" } \\\n";
			$rrdupdatesh .= "{ if ( \$2 == \"processes:\" ) { processes = \$1; } \\\n";
			$rrdupdatesh .= "else if ( \$1 == \"CPU:\" ) { user = \$2; nice = \$4; sys = \$6; interrupt = \$8; } \\\n";
			$rrdupdatesh .= "} END { printf \"N:\"user\":\"nice\":\"sys\":\"interrupt\":\"processes }'`\n\n";

			/* End CPU statistics */

			/* Memory, create Memory statistics database */
			if(! file_exists("$rrddbpath$ifname$mem")) {
				$rrdcreate = "$rrdtool create $rrddbpath$ifname$mem --step $rrdmeminterval ";
				$rrdcreate .= "DS:active:GAUGE:$memvalid:0:10000000 ";
				$rrdcreate .= "DS:inactive:GAUGE:$memvalid:0:10000000 ";
				$rrdcreate .= "DS:free:GAUGE:$memvalid:0:10000000 ";
				$rrdcreate .= "DS:cache:GAUGE:$memvalid:0:10000000 ";
				$rrdcreate .= "DS:wire:GAUGE:$memvalid:0:10000000 ";
				$rrdcreate .= "RRA:MIN:0.5:1:1000 ";
				$rrdcreate .= "RRA:MIN:0.5:5:1000 ";
				$rrdcreate .= "RRA:MIN:0.5:60:1000 ";
				$rrdcreate .= "RRA:MIN:0.5:720:3000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:1:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:5:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:60:1000 ";
				$rrdcreate .= "RRA:AVERAGE:0.5:720:3000 ";
				$rrdcreate .= "RRA:MAX:0.5:1:1000 ";
				$rrdcreate .= "RRA:MAX:0.5:5:1000 ";
				$rrdcreate .= "RRA:MAX:0.5:60:1000 ";
				$rrdcreate .= "RRA:MAX:0.5:720:3000";

				create_new_rrd($rrdcreate);
			}

			/* enter UNKNOWN values in the RRD so it knows we rebooted. */
			if($g['booting']) {
				exec("$rrdtool update $rrddbpath$ifname$mem N:U:U:U:U:U");
			}

 			/* the Memory stats gathering function. */
 			$rrdupdatesh .= "`$sysctl -n vm.stats.vm.v_page_count vm.stats.vm.v_active_count vm.stats.vm.v_inactive_count vm.stats.vm.v_free_count vm.stats.vm.v_cache_count vm.stats.vm.v_wire_count | ";
			$rrdupdatesh .= " $awk '{getline active;getline inactive;getline free;getline cache;getline wire;printf \"$rrdtool update $rrddbpath$ifname$mem N:\"";
			$rrdupdatesh .= "((active/$0) * 100)\":\"((inactive/$0) * 100)\":\"((free/$0) * 100)\":\"((cache/$0) * 100)\":\"(wire/$0 * 100)}'`\n\n";
			
			/* End Memory statistics */

			/* SPAMD, set up the spamd rrd file */
			if (isset($config['installedpackages']['spamdsettings']) &&
				 isset ($config['installedpackages']['spamdsettings']['config'][0]['enablerrd'])) {
				/* set up the spamd rrd file */
				if (!file_exists("$rrddbpath$ifname$spamd")) {
					$rrdcreate = "$rrdtool create $rrddbpath$ifname$spamd --step $rrdspamdinterval ";
					$rrdcreate .= "DS:conn:GAUGE:$spamdvalid:0:10000 ";
					$rrdcreate .= "DS:time:GAUGE:$spamdvalid:0:86400 ";
					$rrdcreate .= "RRA:MIN:0.5:1:1000 ";
					$rrdcreate .= "RRA:MIN:0.5:5:1000 ";
					$rrdcreate .= "RRA:MIN:0.5:60:1000 ";
					$rrdcreate .= "RRA:MIN:0.5:720:3000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:1:1000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:5:1000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:60:1000 ";
					$rrdcreate .= "RRA:AVERAGE:0.5:720:3000 ";
					$rrdcreate .= "RRA:MAX:0.5:1:1000 ";
					$rrdcreate .= "RRA:MAX:0.5:5:1000 ";
					$rrdcreate .= "RRA:MAX:0.5:60:1000 ";
					$rrdcreate .= "RRA:MAX:0.5:720:3000 ";

					create_new_rrd($rrdcreate);
				}

				$rrdupdatesh .= "\n";
				$rrdupdatesh .= "# polling spamd for connections and tarpitness \n";
				$rrdupdatesh .= "$rrdtool update $rrddbpath$ifname$spamd \\\n";
				$rrdupdatesh .= "`$php -q $spamd_gather`\n";

			}
		/* End System statistics */

		$rrdupdatesh .= "sleep 60\n";
		$rrdupdatesh .= "done\n";
		log_error("Creating rrd update script");
		/* write the rrd update script */
		$updaterrdscript = "{$g['vardb_path']}/rrd/updaterrd.sh";
		$fd = fopen("$updaterrdscript", "w");
		fwrite($fd, "$rrdupdatesh");
		fclose($fd);

		/* kill off traffic collectors */
		kill_traffic_collector();

		/* start traffic collector */
		mwexec_bg("/usr/bin/nice -n20 /bin/sh $updaterrdscript");

	} else {
		/* kill off traffic collectors */
		kill_traffic_collector();
	}

	if($g['booting']) 
		echo "done.\n";
		
}

function kill_traffic_collector() {
	mwexec("ps awwwux | grep '/[u]pdaterrd.sh' | awk '{print $2}' | xargs kill");
}

/* This xml 2 array function is courtesy of the php.net comment section on xml_parse.
 * it is roughly 4 times faster then our existing pfSense parser but due to the large
 * size of the RRD xml dumps this is required.
 * The reason we do not use it for pfSense is that it does not know about array fields
 * which causes it to fail on array fields with single items. Possible Todo?
 */
function xml2array($contents, $get_attributes = 1, $priority = 'tag')
{
    if (!function_exists('xml_parser_create'))
    {
        return array ();
    }
    $parser = xml_parser_create('');
    xml_parser_set_option($parser, XML_OPTION_TARGET_ENCODING, "UTF-8");
    xml_parser_set_option($parser, XML_OPTION_CASE_FOLDING, 0);
    xml_parser_set_option($parser, XML_OPTION_SKIP_WHITE, 1);
    xml_parse_into_struct($parser, trim($contents), $xml_values);
    xml_parser_free($parser);
    if (!$xml_values)
        return; //Hmm...
    $xml_array = array ();
    $parents = array ();
    $opened_tags = array ();
    $arr = array ();
    $current = & $xml_array;
    $repeated_tag_index = array ();
    foreach ($xml_values as $data)
    {
        unset ($attributes, $value);
        extract($data);
        $result = array ();
        $attributes_data = array ();
        if (isset ($value))
        {
            if ($priority == 'tag')
                $result = $value;
            else
                $result['value'] = $value;
        }
        if (isset ($attributes) and $get_attributes)
        {
            foreach ($attributes as $attr => $val)
            {
                if ($priority == 'tag')
                    $attributes_data[$attr] = $val;
                else
                    $result['attr'][$attr] = $val; //Set all the attributes in a array called 'attr'
            }
        }
        if ($type == "open")
        {
            $parent[$level -1] = & $current;
            if (!is_array($current) or (!in_array($tag, array_keys($current))))
            {
                $current[$tag] = $result;
                if ($attributes_data)
                    $current[$tag . '_attr'] = $attributes_data;
                $repeated_tag_index[$tag . '_' . $level] = 1;
                $current = & $current[$tag];
            }
            else
            {
                if (isset ($current[$tag][0]))
                {
                    $current[$tag][$repeated_tag_index[$tag . '_' . $level]] = $result;
                    $repeated_tag_index[$tag . '_' . $level]++;
                }
                else
                {
                    $current[$tag] = array (
                        $current[$tag],
                        $result
                    );
                    $repeated_tag_index[$tag . '_' . $level] = 2;
                    if (isset ($current[$tag . '_attr']))
                    {
                        $current[$tag]['0_attr'] = $current[$tag . '_attr'];
                        unset ($current[$tag . '_attr']);
                    }
                }
                $last_item_index = $repeated_tag_index[$tag . '_' . $level] - 1;
                $current = & $current[$tag][$last_item_index];
            }
        }
        elseif ($type == "complete")
        {
            if (!isset ($current[$tag]))
            {
                $current[$tag] = $result;
                $repeated_tag_index[$tag . '_' . $level] = 1;
                if ($priority == 'tag' and $attributes_data)
                    $current[$tag . '_attr'] = $attributes_data;
            }
            else
            {
                if (isset ($current[$tag][0]) and is_array($current[$tag]))
                {
                    $current[$tag][$repeated_tag_index[$tag . '_' . $level]] = $result;
                    if ($priority == 'tag' and $get_attributes and $attributes_data)
                    {
                        $current[$tag][$repeated_tag_index[$tag . '_' . $level] . '_attr'] = $attributes_data;
                    }
                    $repeated_tag_index[$tag . '_' . $level]++;
                }
                else
                {
                    $current[$tag] = array (
                        $current[$tag],
                        $result
                    );
                    $repeated_tag_index[$tag . '_' . $level] = 1;
                    if ($priority == 'tag' and $get_attributes)
                    {
                        if (isset ($current[$tag . '_attr']))
                        {
                            $current[$tag]['0_attr'] = $current[$tag . '_attr'];
                            unset ($current[$tag . '_attr']);
                        }
                        if ($attributes_data)
                        {
                            $current[$tag][$repeated_tag_index[$tag . '_' . $level] . '_attr'] = $attributes_data;
                        }
                    }
                    $repeated_tag_index[$tag . '_' . $level]++; //0 and 1 index is already taken
                }
            }
        }
        elseif ($type == 'close')
        {
            $current = & $parent[$level -1];
        }
    }
    return ($xml_array);
}

?>