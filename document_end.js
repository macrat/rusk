var SCROLL_STEP = 24;
var ZOOM_STEP = 0.1;


function zoomSet(val)
{
	document.body.style.zoom = val;
}


function zoomTo(val)
{
	zoomSet(parseFloat(document.body.style.zoom || 1) + val);
}


function onKeyPress(ev)
{
	if(ev.ctrlKey)
	{
		switch(ev.string)
		{
			case 'f':
				history.forward();
				break;
			case 'b':
				history.back();
				break;

			case 'r':
				location.reload(true);
				break;
			case 'R':
				location.reload(false);
				break;

			case 'j':
				window.scrollBy(0,  SCROLL_STEP);
				break;
			case 'k':
				window.scrollBy(0, -SCROLL_STEP);
				break;
			case 'h':
				window.scrollBy(-SCROLL_STEP, 0);
				break;
			case 'l':
				window.scrollBy( SCROLL_STEP, 0);
				break;

			case 'plus':
				zoomTo( ZOOM_STEP);
				break;
			case 'minus':
				zoomTo(-ZOOM_STEP);
				break;
			case '0':
				zoomSet(0);
				break;
		}
	}
}
document.addEventListener("keypress", onKeyPress, false);
