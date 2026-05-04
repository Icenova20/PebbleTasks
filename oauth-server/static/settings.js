(function () {
  function getQueryParam(variable, defaultValue) {
    var query = location.search ? location.search.substring(1) : '';
    var vars = query ? query.split('&') : [];
    for (var i = 0; i < vars.length; i++) {
      var part = vars[i];
      var eq = part.indexOf('=');
      var key = eq >= 0 ? part.slice(0, eq) : part;
      var keyMatch = false;
      try {
        keyMatch = decodeURIComponent(key) === variable;
      } catch (e) {
        keyMatch = key === variable;
      }
      if (!keyMatch) {
        continue;
      }
      if (eq < 0) {
        return '';
      }
      var rawV = part.slice(eq + 1).split('+').join(' ');
      try {
        return decodeURIComponent(rawV);
      } catch (e2) {
        return rawV;
      }
    }
    return defaultValue !== undefined && defaultValue !== null ? defaultValue : false;
  }
  var settingsBase = document.body.getAttribute('data-settings-base') || '';
  var rt = getQueryParam('return_to', 'pebblejs://close#');
  if (typeof rt !== 'string' || !rt) {
    rt = 'pebblejs://close#';
  }
  var oauthStartUrl = settingsBase + '/oauth/start?return_to=' + encodeURIComponent(rt);
  var oauthIn = document.getElementById('oauthUrl');
  if (oauthIn) {
    oauthIn.value = oauthStartUrl;
  }
  var modes = document.getElementsByName('mode');
  var themeRow = document.getElementById('watchThemeRow');
  var themes = themeRow ? themeRow.querySelectorAll('input[name="watch_theme"]') : [];
  var curMode = getQueryParam('current_mode', 'local');
  /* Pebble WebViews sometimes omit location.search; server sets data-theme-preset on <body>. */
  var curThemeNum = 0;
  var dataPreset =
    document.body && document.body.getAttribute('data-theme-preset') !== null
      ? document.body.getAttribute('data-theme-preset')
      : '';
  if (dataPreset === '0' || dataPreset === '1') {
    curThemeNum = parseInt(dataPreset, 10);
  } else {
    var curThemeRaw = getQueryParam('theme_preset', '0');
    curThemeNum = parseInt(curThemeRaw, 10);
    if (isNaN(curThemeNum) || curThemeNum < 0 || curThemeNum > 1) {
      curThemeNum = 0;
    }
  }
  if (curMode === 'google' || curMode === 'local') {
    for (var m = 0; m < modes.length; m++) {
      if (modes[m].value === curMode) {
        modes[m].checked = true;
      }
    }
  }
  var pickTheme = curThemeNum === 1 ? 'dark' : 'light';
  for (var th = 0; th < themes.length; th++) {
    themes[th].checked = themes[th].value === pickTheme;
  }
  if (document.body) {
    if (curMode === 'google') {
      document.body.className = 'mode-google';
    } else if (curMode === 'local') {
      document.body.className = 'mode-local';
    }
  }
  (function () {
    var s = getQueryParam('signed_in', '0');
    var linked = s === '1' || s === 'true';
    var st0 = document.getElementById('signedStatus');
    var nd0 = document.getElementById('needTokenHint');
    if (linked) {
      if (st0) {
        st0.style.display = '';
      }
      if (nd0) {
        nd0.style.display = 'none';
      }
    } else {
      if (st0) {
        st0.style.display = 'none';
      }
      if (nd0) {
        nd0.style.display = '';
      }
    }
  })();
  function sync() {
    var g = false;
    for (var i = 0; i < modes.length; i++) {
      if (modes[i].checked && modes[i].value === 'google') {
        g = true;
      }
    }
    if (document.body) {
      document.body.className = g ? 'mode-google' : 'mode-local';
    }
  }
  for (var j = 0; j < modes.length; j++) {
    modes[j].addEventListener('change', sync);
  }
  sync();
  document.getElementById('copyOauth').onclick = function () {
    var el = document.getElementById('oauthUrl');
    if (!el) return;
    el.focus();
    el.select();
    try {
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(el.value);
        alert(
          'Link copied. Open Safari, Chrome, or your phone’s default browser (not Pebble’s embedded browser), paste in the address bar, complete Google sign-in, then paste the PEBBLETASKS1 line here.'
        );
        return;
      }
    } catch (e) {}
    try {
      if (document.execCommand('copy')) {
        alert(
          'Copied. Open your native browser (not Pebble), paste the link, complete sign-in, then paste the PEBBLETASKS1 line here.'
        );
        return;
      }
    } catch (e2) {}
    alert(
      'Select the link field, copy, then paste it in your native browser’s address bar (not inside Pebble).'
    );
  };
  document.getElementById('save').onclick = function () {
    var mode = 'local';
    for (var i = 0; i < modes.length; i++) {
      if (modes[i].checked) mode = modes[i].value;
    }
    var themePreset = 0;
    for (var jt = 0; jt < themes.length; jt++) {
      if (themes[jt].checked && themes[jt].value === 'dark') {
        themePreset = 1;
      }
    }
    if (themes.length === 0) {
      themePreset = curThemeNum;
    }
    if (!rt) {
      alert('Missing return_to — open settings from the Pebble app.');
      return;
    }
    var payload = {
      mode: mode,
      themePreset: themePreset,
      theme: themePreset === 1 ? 'dark' : 'light',
    };
    if (rt.indexOf('#') >= 0) {
      document.location = rt + encodeURIComponent(JSON.stringify(payload));
    } else {
      document.location = rt + '#' + encodeURIComponent(JSON.stringify(payload));
    }
  };
  function parsePebbleAuthPaste(s) {
    s = (s || '').replace(/^\s+|\s+$/g, '');
    if (!s) {
      return null;
    }
    if (s.indexOf('PEBBLETASKS1') === 0) {
      try {
        return JSON.parse(atob(s.slice('PEBBLETASKS1'.length)));
      } catch (e) {
        return null;
      }
    }
    try {
      return JSON.parse(s);
    } catch (a) {}
    try {
      return JSON.parse(decodeURIComponent(s));
    } catch (b) {}
    return null;
  }
  var authSave = document.getElementById('authPasteSave');
  if (authSave) {
    authSave.onclick = function () {
      var t =
        (document.getElementById('authPaste') &&
          document.getElementById('authPaste').value) ||
        '';
      var o = parsePebbleAuthPaste(t);
      if (!o || !o.access_token) {
        alert(
          'Could not read a Google token. Paste PEBBLETASKS1… (base64 JSON) or full JSON with access_token.'
        );
        return;
      }
      if (!rt) {
        alert('Missing return_to — open settings from the Pebble app.');
        return;
      }
      if (!o.mode) {
        o.mode = 'google';
      }
      var enc = encodeURIComponent(JSON.stringify(o));
      if (rt.indexOf('#') >= 0) {
        document.location = rt + enc;
      } else {
        document.location = rt + '#' + enc;
      }
    };
  }
})();
