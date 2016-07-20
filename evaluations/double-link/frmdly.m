function frame_delays = frmdly( packet_delays )
% FRMDLY    extracts frame delays from the packet list, TODO
% packet_delays: list of packets 
% frame_delays: list of frames
% for rebera we call like: frmdly( reberapac(:,[5 2 4]) )
% [1waydelay,seq_no,frame_no]
% for facetime we call like: [pac(:,[2 3 4 5]),pac(:,1)]
% [1waydelay,seq_no,length,ts,sendingtime]
% ==========================================================

if size(packet_delays,2) ~= 3 % belongs to facetime
    marker_column = 4; % marker is a timestamp>0
else % rebera
    marker_column = 3; % marker is frame index>=0
end

[~,I] = sort( packet_delays(:,marker_column) );
packet_delays = packet_delays(I,:);  
frame_delays = zeros( sum(diff(packet_delays(:,marker_column))>0)+1, 1 );

this_marker = -1; frames_encountered = 0;
for i = 1:size(packet_delays,1)

    % mark the largest 1-way delay of packets that make up each frame
    if packet_delays(i,marker_column) == this_marker
        frame_delays(frames_encountered) = max( frame_delays(frames_encountered), packet_delays(i,1) );
    else
        this_marker = packet_delays(i,marker_column);
        frames_encountered = frames_encountered+1;
        frame_delays(frames_encountered) = packet_delays(i,1);
    end
end
frame_delays = frame_delays(1:frames_encountered);
return