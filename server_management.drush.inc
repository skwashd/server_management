<?php

/**
 * @file
 * Server Management drush plugin functions.
 */

/**
 * Implements hook_drush_help().
 */
function server_management_drush_help($command) {
  switch ($command) {
    case 'drush:sm-linux-type':
      return dt('Determine the GNU/Linux distro type.');
    case 'drush:sm-apache-file':
      return dt('Determine the full path to the Apache config file for a virtual host.');
    case 'drush:sm-apache-graceful':
      return dt('Restarts the apache httpd process gracefully.');
  }
}

/**
 * Implements hook_drush_command().
 */
function server_management_drush_command() {
  $items = array();

  $items['sm-apache-file'] = array(
    'description' => dt('Get the path to the Apache config file for a vhost.'),
    'arguments'   => array(),
    'options'   => array(
      'vhost' => array(
        'description' => dt('The domain name of the virtual host file to lookup'),
        'example_value' => 'example.com',
        'value' => 'required',
      ),
    ),
    'examples' => array(
      'Standard example' => 'drush sm-apache-file --vhost=example.com',
    ),
    'aliases' => array('apache-file'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );

  $items['sm-apache-graceful'] = array(
    'description' => dt('Restart apache gracefully'),
    'arguments' => array(),
    'examples' => array(
      'Standard example' => 'drush sm-apache-graceful',
    ),
    'aliases' => array('apache-graceful', 'apache-reload'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );

  $items['sm-linux-type'] = array(
    'description' => dt('Get the Linux distro type.'),
    'arguments'   => array(),
    'examples' => array(
      'Standard example' => 'drush sm-linux-type',
    ),
    'aliases' => array('linux-type'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );

  return $items;
}

/**
 * Drush apache file command callback.
 */
function drush_server_management_sm_apache_file() {
  $host = drush_get_option('vhost');
  if (!$host) {
    $host = drush_get_option('uri');
  }

  try {
    $full_path = server_management_get_apache_file($host);
  }
  catch (Exception $e) {
    drush_die((string) $e);
  }

  drush_print(dt('Apache file: @full_path', array('@full_path' => $full_path)));
  drush_print_pipe($full_path);
}

/**
 * Apache graceful command callback.
 */
function drush_server_management_sm_apache_graceful() {

  switch(_server_management_get_distro_identifier()) {
    case 'debian':
      $apachectl_cmd = '/usr/sbin/apache2ctl';
      $service_name = 'apache2';
      break;
    case 'redhat':
      $apachectl_cmd = '/usr/sbin/apachectl';
      $service_name = 'httpd';
      break;

    default:
      drush_die(dt('Environment not supported'));
  }

  if (!is_executable($apachectl_cmd)) {
    drush_die(dt('Unable to execute command @cmd.', array('@cmd' => $apachectl_cmd)));
  }

  if (!drush_shell_exec($apachectl_cmd . ' configtest')) {
    drush_die(dt('Apache configuration is invalid.'));
  }

  if (!drush_shell_exec('sudo /usr/sbin/service ' . $service_name . ' graceful')) {
    drush_die(dt('Apache failed to reload. Check state of the apache service.'));
  }

  drush_print(dt('Apache restarted gracefully.'));
}

/**
 * Drush Linux type command callback.
 */
function drush_server_management_sm_linux_type() {
  $id = _server_management_get_distro_identifier(FALSE);
  if (FALSE === $id) {
    drush_die(dt('Unable to find lsb_release command. Unable to determine distro.'));
  }

  drush_print(dt('Linux type: @id', array('@id' => $id)));
  drush_print_pipe($id);
}

/**
 * Wrapper for PHP's readlink() to make it function like GNU "readlink -e".
 */
function readlink_e($link) {
  $target = realpath($link);
  if (FALSE === $target) {
    return $link;
  }

  if (is_link($target)) {
    return readlink_e($target);
  }
  return $target;
}

/**
 * Fetches the fully resolved path to an apache config file.
 *
 * @param $host string
 *   The hostname to lookup.
 *
 * @return string
 *   The fully qualified path to the apache config file.
 *
 * @throws Exception
 */
function server_management_get_apache_file($host) {

  if (!$host) {
    throw new Exception('No virtual host specified.');
  }

  $path_string = '';
  switch(_server_management_get_distro_identifier()) {
    case 'debian':
      $path_string = '/etc/apache2/sites-enabled/@vhost';
      break;
    case 'redhat':
      $path_string = '/etc/httpd/conf.d/@vhost.conf';
      break;
  }

  if (!$path_string) {
    throw new Exception('Linux distro not supported.');
  }

  $config_file = dt($path_string, array('@vhost' => $host));
  $full_path = readlink_e($config_file);

  return $full_path;
}

/**
 * Determine the Linux distro the command is running on.
 *
 * @param $return_raw bool
 *   Return the raw ungrouped output?
 */
function _server_management_get_distro_identifier($return_raw = FALSE) {
  if (!drush_shell_exec('lsb_release -i')) {
    return FALSE;
  }

  $cmd_out = drush_shell_exec_output();
  if (!isset($cmd_out[0])) {
    return FALSE;
  }

  $m = array();
  if (!preg_match('/Distributor ID:\\t(.+)/', $cmd_out[0], $m) || empty($m[1])) {
    return FALSE;
  }

  if ($return_raw) {
    return $m[1];
  }

  switch(strtolower($m[1])) {
    case 'debian':
    case 'ubuntu':
      return 'debian';

    case 'centos':
    case 'fedora':
    case 'redhatenterpriseserver':
      return 'redhat';
  }

  return 'Unknown';
}


