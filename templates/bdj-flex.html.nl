<!-- Nederlands Flex -->
<!DOCTYPE html>
<!--
http://danielstern.ca/range.css/#/
-->
<!-- VERSION 2022-1-17 -->
<html>
<head>
  <title>BDJ4</title>
  <meta http-equiv="Content-Type" content="text/html;charset=utf-8" >
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="Description" content="BallroomDJ Web Interface">
  <style type="text/css">
    body {
      background-color: white;
      margin: 0px;
      padding: 0px;
      font-size: 100%;
    }
    .mainbox {
      display: flex;
      flex-direction: column;
      height: 95%;
      width: 95%;
      padding-left: 10pt;
      padding-top: 5pt;
    }
    .dispbox {
      display: flex;
      margin: auto;
      width: 100%;
      max-width: 450px;
      flex-grow: 1;
      flex-shrink: 1;
      flex-direction: column;
      justify-content: flex-start;
    }
    .linebox {
      margin-top: 5pt;
      display: flex;
      flex-direction: row;
      flex-wrap: nowrap;
      flex-grow: 1;
      flex-shrink: 2;
      justify-content: flex-start;
    }
    .expand {
      justify-content: space-between;
    }
    .flexjend {
      justify-content: flex-end !important;
    }
    .rightbox {
      display: flex;
      flex-direction: row;
      justify-content: flex-start;
      align-self: flex-end;
      flex-grow: 0;
      flex-shrink: 0;
    }
    .vcenter {
      margin-top: auto;
      margin-bottom: auto;
    }
    .padlr {
      padding-left: 3pt;
      padding-right: 3pt;
    }
    .mleftauto {
      margin-left: auto;
    }
    .mpad {
      margin-left: 1pt;
    }
    .mpadr {
      margin-right: 2pt;
    }
    .fright {
      float: right;
    }
    .tright {
      text-align: right;
    }
    .grow {
      flex-grow: 2;
      flex-shrink: 2;
    }
    .stretch {
      align-self: stretch;
    }
    .withborder {
      border: 1pt solid #d5b5ff;
    }
    p {
      margin: 0;
      padding: 0;
      font-size: 100%;
    }
    .pbold {
      font-weight: bold;
    }
    img {
      border: 0;
    }
    .imgmiddle {
      align: middle;
    }
    button {
      padding-top: 7pt;
      padding-bottom: 7pt;
      border-radius: 0;
      -webkit-border-radius: 0;
      -webkit-appearance: none;
      background-color: #d5b5ff;
    }
    input.button:focus {
      outline-width: 0;
      color: black;
    }
    hr {
      background-color: #d5b5ff;
    }
    select {
      background-color: #d5b5ff;
      appearance: none;
    }
    input {
      outline: none;
      padding: 0.5em;
      font-size: 100%;
      border-radius: 0;
      background-color: #ffffff;
      -webkit-border-radius: 0;
      -webkit-appearance: none;
    }
    .muteb {
      padding: 0;
    }
    .divshown {
      display: -webkit-flex;
      display: flex;
    }
    .divinline {
      display: inline;
      clear: none;
    }
    .divnone {
      display: none;
      clear: none;
    }
    .wb {
      width: 100%;
      background-color: #d5b5ff;
    }
    input[type=range]:focus {
      outline: none;
    }
    input[type=range]::-webkit-slider-runnable-track {
      width: 100%;
      height: 8.4px;
      cursor: pointer;
      box-shadow: 0px 0px 1px #000000, 0px 0px 0px #0d0d0d;
      background: #9056a5;
      border-radius: 4.5px;
      border: 0px solid #010101;
    }
    input[type=range]::-webkit-slider-thumb {
      box-shadow: 1px 1px 2px #9056a5, 0px 0px 1px #9b66af;
      border: 0.3px solid rgba(144, 86, 165, 0.84);
      height: 11px;
      width: 50px;
      border-radius: 47px;
      background: rgba(255, 0, 117, 0.53);
      cursor: pointer;
      -webkit-appearance: none;
      margin-top: -1.3px;
    }
    input[type=range]:focus::-webkit-slider-runnable-track {
      background: #9b66af;
    }
    input[type=range]::-moz-range-track {
      width: 100%;
      height: 8.4px;
      cursor: pointer;
      box-shadow: 0px 0px 1px #000000, 0px 0px 0px #0d0d0d;
      background: #9056a5;
      border-radius: 4.5px;
      border: 0px solid #010101;
    }
    input[type=range]::-moz-range-thumb {
      box-shadow: 1px 1px 2px #9056a5, 0px 0px 1px #9b66af;
      border: 0.3px solid rgba(144, 86, 165, 0.84);
      height: 11px;
      width: 50px;
      border-radius: 47px;
      background: rgba(255, 0, 117, 0.53);
      cursor: pointer;
    }
    input[type=range]::-ms-track {
      width: 100%;
      height: 8.4px;
      cursor: pointer;
      background: transparent;
      border-color: transparent;
      color: transparent;
    }
    input[type=range]::-ms-fill-lower {
      background: #814d94;
      border: 0px solid #010101;
      border-radius: 9px;
      box-shadow: 0px 0px 1px #000000, 0px 0px 0px #0d0d0d;
    }
    input[type=range]::-ms-fill-upper {
      background: #9056a5;
      border: 0px solid #010101;
      border-radius: 9px;
      box-shadow: 0px 0px 1px #000000, 0px 0px 0px #0d0d0d;
    }
    input[type=range]::-ms-thumb {
      box-shadow: 1px 1px 2px #9056a5, 0px 0px 1px #9b66af;
      border: 0.3px solid rgba(144, 86, 165, 0.84);
      height: 11px;
      width: 50px;
      border-radius: 47px;
      background: rgba(255, 0, 117, 0.53);
      cursor: pointer;
      height: 8.4px;
    }
    input[type=range]:focus::-ms-fill-lower {
      background: #9056a5;
    }
    input[type=range]:focus::-ms-fill-upper {
      background: #9b66af;
    }
    input[type=range] {
      -webkit-appearance: none;
      margin: 1.3px 0;
      width: 50%;
    }
    input[type=range]:focus {
      outline: none;
    }
    input[type=range]::-webkit-slider-runnable-track {
      width: 100%;
      height: 8.4px;
      cursor: pointer;
      box-shadow: 0px 0px 1px #000000, 0px 0px 0px #0d0d0d;
      background: #9056a5;
      border-radius: 4.5px;
      border: 0px solid #010101;
    }
    input[type=range]::-webkit-slider-thumb {
      box-shadow: 1px 1px 2px #9056a5, 0px 0px 1px #9b66af;
      border: 0.3px solid rgba(144, 86, 165, 0.84);
      height: 11px;
      width: 50px;
      border-radius: 47px;
      background: rgba(255, 0, 117, 0.53);
      cursor: pointer;
      -webkit-appearance: none;
      margin-top: -1.3px;
    }
    input[type=range]:focus::-webkit-slider-runnable-track {
      background: #9b66af;
    }
    input[type=range]::-moz-range-track {
      width: 100%;
      height: 8.4px;
      cursor: pointer;
      box-shadow: 0px 0px 1px #000000, 0px 0px 0px #0d0d0d;
      background: #9056a5;
      border-radius: 4.5px;
      border: 0px solid #010101;
    }
    input[type=range]::-moz-range-thumb {
      box-shadow: 1px 1px 2px #9056a5, 0px 0px 1px #9b66af;
      border: 0.3px solid rgba(144, 86, 165, 0.84);
      height: 11px;
      width: 50px;
      border-radius: 47px;
      background: rgba(255, 0, 117, 0.53);
      cursor: pointer;
    }
    input[type=range]::-ms-track {
      width: 100%;
      height: 8.4px;
      cursor: pointer;
      background: transparent;
      border-color: transparent;
      color: transparent;
    }
    input[type=range]::-ms-fill-lower {
      background: #814d94;
      border: 0px solid #010101;
      border-radius: 9px;
      box-shadow: 0px 0px 1px #000000, 0px 0px 0px #0d0d0d;
    }
    input[type=range]::-ms-fill-upper {
      background: #9056a5;
      border: 0px solid #010101;
      border-radius: 9px;
      box-shadow: 0px 0px 1px #000000, 0px 0px 0px #0d0d0d;
    }
    input[type=range]::-ms-thumb {
      box-shadow: 1px 1px 2px #9056a5, 0px 0px 1px #9b66af;
      border: 0.3px solid rgba(144, 86, 165, 0.84);
      height: 11px;
      width: 50px;
      border-radius: 47px;
      background: rgba(255, 0, 117, 0.53);
      cursor: pointer;
      height: 8.4px;
    }
    input[type=range]:focus::-ms-fill-lower {
      background: #9056a5;
    }
    input[type=range]:focus::-ms-fill-upper {
      background: #9b66af;
    }
  </style>
</head>
<body>
  <div class="mainbox">
      <div id="top" class="dispbox">
        <div class="linebox expand">
          <p><img
            src="ballroomdj4.svg" width="200" alt="BDJ4"></p>
          <img id="nextpagei" onclick="javascript:bdj.nextPage();"
            src="mrc/light/nextpage.svg" alt="next page">
        </div>
      </div>
      <div id="controlspage1" class="dispbox">
        <div class="linebox">
          <p class="vcenter">Volume</p>
          <input class="stretch grow padlr"
              id="volumeb" type="range"
              onmousedown="javascript:bdj.chgStart();"
              onmouseup="javascript:bdj.chgEnd();"
              oninput="javascript:bdj.chgVolume();"
              onchange="javascript:bdj.chgVolume();">
          <div class="rightbox vcenter">
            <p class="mleftauto vcenter tright mpadr" id="vold">100%</p>
            <button class="muteb" id="muteb"
                type="button"
                onclick="javascript:bdj.chgMute();"><img
                id="mutei" align="middle" src="mrc/light/mute.svg"
                alt="mute"></button>
          </div>
        </div>
        <div class="linebox">
          <input class="button stretch grow wb" type="submit"
              onclick="javascript:bdj.sendCmd('play');"
              name="playpause" value="Afspelen / Pauseren">
          <div class="rightbox vcenter">
            <img class="mpad" id="playstatusi"
                  src="mrc/light/stop.svg" alt="stop">
            <div class="divshown" id="pausestatusd">
              <img class="mpad mpadr" id="pausestatusi"
                  src="mrc/light/pause.svg" alt="pause">
            </div>
          </div>
        </div>
        <div class="linebox">
          <input class="button stretch grow wb" type="submit" id="fadeb"
              onclick="javascript:bdj.sendCmd('fade');"
              name="fade" value="Fade" msgstr "Fade-In tijd" msgstr "Fade-Out tijd" msgstr "Fade type">
          <div class="rightbox vcenter"></div>
        </div>
        <div class="linebox">
          <input class="button stretch grow wb" type="submit"
              onclick="javascript:bdj.sendCmd('repeat');"
              name="repeat" value="Herhalen">
          <div class="rightbox vcenter">
            <div class="divshown" id="repeatstatusd">
              <img class="mpad mpadr" id="repeatstatusi"
                  src="mrc/light/repeat.svg" alt="repeat">
            </div>
          </div>
        </div>
        <div class="linebox">
          <input class="button stretch grow wb" type="submit"
              onclick="javascript:bdj.sendCmd('pauseatend');"
              name="pauseatend" value="Pauzeer aan einde">
          <div class="rightbox vcenter">
            <img class="mpad" id="pauseatendi"
                src="led_off.svg" alt="pause-at-end">
          </div>
        </div>
        <div class="linebox">
          <input class="button stretch grow wb" type="submit"
              onclick="javascript:bdj.sendCmd('nextsong');"
              name="nextsong" value="Volgende nummer">
          <div class="rightbox vcenter"></div>
        </div>
        <div class="linebox">
          <p class="vcenter">Speed</p>
          <input class="stretch grow padlr"
              id="speedb" type="range"
              min="70" max="130" value="100"
              onmousedown="javascript:bdj.chgStart();"
              onmouseup="javascript:bdj.chgEnd();"
              oninput="javascript:bdj.chgSpeed();"
              onchange="javascript:bdj.chgSpeed();">
          <div class="rightbox vcenter flexjend">
            <p class="mpad tright vcenter" id="speedd">100%</p>
          </div>
        </div>
        <div class="linebox">
          <p class="vcenter stretch grow padlr"><span
          class="pbold" id="dance"></span>
          <span class="pbold fright"><span
            id="playedtime"></span>&nbsp;<span
            id="dursep"></span>&nbsp;<span
            id="duration"></span></p>
          <div class="flexjend rightbox vcenter">
            <p class="mpad tright"></p>
          </div>
        </div>
        <div class="linebox">
          <p class="vcenter stretch grow padlr" id="artist"></p>
        </div>
        <div class="linebox">
          <p class="vcenter stretch grow padlr" id="title"></p>
        </div>
      </div>
      <div id="controlspage2" class="divnone">
        <div class="linebox">
          <p class="txtcenter mleftauto pbold heading">Quick Play</p>
        </div>
        <div class="linebox">
          <select id="dancelist" name="dancelist"></select>
        </div>
        <div class="linebox">
          <input class="stretch grow wb" type="submit"
              onclick="javascript:bdj.sendDanceCmd('queue');"
              name="queue" value="Wachtrij" msgstr "" msgstr "" msgstr "" msgstr "" msgstr "Wachtrij lengte" msgstr "">
        </div>
        <div class="linebox">
          <input class="stretch grow wb" type="submit"
              onclick="javascript:bdj.sendDanceCmd('queue5');"
              name="queue5" value="Queue 5">
        </div>
        <div class="linebox">
          <input class="stretch grow wb" type="submit"
              onclick="javascript:bdj.sendDanceCmd('clear');"
              name="clear" value="Clear Queue">
        </div>
        <div class="linebox">
          <p class="stretch grow wb withborder"></p>
        </div>
        <div class="linebox">
          <select id="playlistsel" name="playlistsel"></select>
        </div>
        <div class="linebox">
          <input class="stretch grow wb" type="submit"
            onclick="javascript:bdj.sendPLCmd('playlistclearplay');"
            name="plclearplay" value="Clear & Play">
          <input class="stretch grow wb" type="submit"
            onclick="javascript:bdj.sendPLCmd('playlistqueue');"
            name="plqueue" value="Wachtrij" msgstr "" msgstr "" msgstr "" msgstr "" msgstr "Wachtrij lengte" msgstr "">
        </div>
    </div>
  </div>

  <script type="text/javascript">
var bdj = {};

bdj.updIntervalId = '';
bdj.chgInUse = false;
bdj.currentPage = 1;
bdj.maxPages = 2;
bdj.speedIntervalId = '';
bdj.volumeIntervalId = '';
bdj.muteState = 0;

bdj.chgStart = function () {
  bdj.chgInUse = true;
}
bdj.chgEnd = function () {
  bdj.chgInUse = false;
}
bdj.chgMute = function () {
  bdj.muteState = 1-bdj.muteState;
  var ob = document.getElementById("mutei");
  if (bdj.muteState == 0) {
    ob.src = 'mrc/light/mute.svg';
  } else {
    ob.src = 'mrc/light/unmute.svg';
  }
  bdj.sendCmd ('volmute');
}
bdj.chgVolume = function () {
  var oi = document.getElementById("volumeb");
  var o = document.getElementById("vold");
  o.innerHTML = oi.value.toString()+'%';
  if (bdj.IntervalId != '') {
    clearInterval (bdj.volumeIntervalId);
  }
  bdj.volumeIntervalId = setInterval(bdj._chgVolume,100);
}

bdj._chgVolume = function () {
  var o = document.getElementById("volumeb");
  bdj.sendCmd ('volume '+o.value);
  clearInterval (bdj.volumeIntervalId);
  bdj.volumeIntervalId = '';
}

bdj.chgSpeed = function () {
  var oi = document.getElementById("speedb");
  var o = document.getElementById("speedd");
  o.innerHTML = oi.value.toString()+'%';
  if (bdj.speedIntervalId != '') {
    clearInterval (bdj.speedIntervalId);
  }
  bdj.speedIntervalId = setInterval(bdj._chgSpeed,100);
}

bdj._chgSpeed = function () {
  var o = document.getElementById("speedb");
  bdj.sendCmd ('speed '+o.value);
  clearInterval (bdj.speedIntervalId);
  bdj.speedIntervalId = '';
}

bdj.nextPage = function () {
  var p = "controlspage"+bdj.currentPage;
  var o = document.getElementById (p);
  if ( o ) {
    o.className = "divnone";
  }
  ++bdj.currentPage;
  if (bdj.currentPage > bdj.maxPages) {
    bdj.currentPage = 1;
  }
  p = "controlspage"+bdj.currentPage;
  o = document.getElementById (p);
  if ( o ) {
    o.className = "dispbox";
  }
}

bdj.sendDanceCmd = function (cmd) {
  var o = document.getElementById("dancelist");
  bdj.sendCmd (cmd + ' ' + o.options[o.selectedIndex].value);
}

bdj.sendPLCmd = function (cmd) {
  var o = document.getElementById("playlistsel");
  bdj.sendCmd (cmd + ' ' + o.options[o.selectedIndex].value);
}

bdj.sendCmd = function (cmd) {
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      // sending a command will get the same response back as getstatus
      bdj.updateData (xhr.responseText);
    }
  };
  xhr.open('GET', '/cmd?'+cmd, true);
  xhr.send();
}

bdj.updateDanceList = function (data) {
  var o = document.getElementById("dancelist");
  if (o) {
    o.innerHTML = data;
  }
}

bdj.updatePlayListSel = function (data) {
  var o = document.getElementById("playlistsel");
  if (o) {
    o.innerHTML = data;
  }
}

bdj.updateData = function (data) {
  var jd = JSON.parse (data);
  var o = document.getElementById("playstatusi");
  o.src = 'mrc/light/'+jd.playstate+".svg";

  o = document.getElementById("pausestatusd");
  if (jd.willpause == 1) {
    o.className = "divshown";
  } else {
    o.className = "divnone";
  }

  o = document.getElementById("repeatstatusd");
  if (jd.repeat == 1) {
    o.className = "divshown";
  } else {
    o.className = "divnone";
  }

  o = document.getElementById("pauseatendi");
  o.src = "led_off.svg";
  if (jd.pauseatend == 1) {
    o.src = "led_on.svg";
  }

  if (jd.vol) {
    o = document.getElementById("vold");
    o.innerHTML = jd.vol;
    o = document.getElementById("volumeb");
    jd.vol = jd.vol.replace(new RegExp("%$"), "").trim();
    o.value = jd.vol;
  }

  if (jd.speed) {
    o = document.getElementById("speedd");
    o.innerHTML = jd.speed;
    o = document.getElementById("speedb");
    jd.speed = jd.speed.replace(new RegExp("%$"), "").trim();
    o.value = jd.speed;
  }

  o = document.getElementById("dance");
  o.innerHTML = jd.dance;
  o = document.getElementById("artist");
  o.innerHTML = jd.artist;
  o = document.getElementById("title");
  o.innerHTML = jd.title;
  if (jd.duration != "0:00") {
    o = document.getElementById("playedtime");
    o.innerHTML = jd.playedtime;
    o = document.getElementById("dursep");
    o.innerHTML = '/';
    o = document.getElementById("duration");
    o.innerHTML = jd.duration;
  } else {
    o = document.getElementById("playedtime");
    o.innerHTML = '';
    o = document.getElementById("dursep");
    o.innerHTML = '';
    o = document.getElementById("duration");
    o.innerHTML = '';
  }
}

bdj.getDanceList = function () {
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      bdj.updateDanceList (xhr.responseText);
    }
  };
  xhr.open('GET', '/getdancelist', true);
  xhr.send();
}

bdj.getPlayListSel = function () {
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      bdj.updatePlayListSel (xhr.responseText);
    }
  };
  xhr.open('GET', '/getplaylistsel', true);
  xhr.send();
}

bdj.doUpdate = function () {
  if (bdj.chgInUse) {
    // don't run an update if the user is currently messing with
    // one of the sliders.
    return;
  }
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      bdj.updateData (xhr.responseText);
    }
  };
  xhr.open('GET', '/getstatus', true);
  xhr.send();
}
bdj.setSize = function (o,s) {
  o.height = s;
  o.width = s;
}
bdj.doLoad = function () {
  var o = document.getElementById("fadeb");
  var s = o.clientHeight;
  o = document.getElementById("mutei");
  o.width = s;
/*  bdj.setSize(o,s); */
  o = document.getElementById("pausestatusi");
  bdj.setSize(o,s);
  o = document.getElementById("playstatusi");
  bdj.setSize(o,s);

  o = document.getElementById("repeatstatusi");
  bdj.setSize(o,s);
  o = document.getElementById("pauseatendi");
  bdj.setSize(o,s);
  o = document.getElementById("nextpagei");
  bdj.setSize(o,s);

  o = document.getElementById("pausestatusd");
  o.className = 'divshown';

  o = document.getElementById("muteb");
  var s2 = o.clientWidth;
  o = document.getElementById("vold");
  s2 += o.clientWidth;
  var cstyle = getComputedStyle(o);
  s2 += 4 * parseInt(cstyle.marginLeft)
  var rblist = document.getElementsByClassName("rightbox");
  var i;
  for (i = 0; i < rblist.length; i++) {
    rblist[i].style.width = s2 + 'px';
  }

  if (bdj.updIntervalId == '') {
    bdj.getDanceList();
    bdj.getPlayListSel();
    bdj.doUpdate();
    bdj.updIntervalId = setInterval(bdj.doUpdate,300);
  }
}

window.onload = function () { bdj.doLoad(); };
window.onresize = function () { bdj.doLoad(); };
  </script>
</body>
</html>
