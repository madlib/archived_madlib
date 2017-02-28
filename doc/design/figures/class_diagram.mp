% Licensed to the Apache Software Foundation (ASF) under one
% or more contributor license agreements.  See the NOTICE file
% distributed with this work for additional information
% regarding copyright ownership.  The ASF licenses this file
% to you under the Apache License, Version 2.0 (the
% "License"); you may not use this file except in compliance
% with the License.  You may obtain a copy of the License at
%
%   http://www.apache.org/licenses/LICENSE-2.0
%
% Unless required by applicable law or agreed to in writing,
% software distributed under the License is distributed on an
% "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
% KIND, either express or implied.  See the License for the
% specific language governing permissions and limitations
% under the License.

% Metapost figure compiled offline to create the class diagram for modular fold/reduce
% need MetaUML (https://github.com/ogheorghies/MetaUML) to compile this file
% to create pdf figure: mptopdf class_diagram.mp

input metauml;
beginfig(1);
iClass.foreColor := (.9, .9, .8);  % global color for all classes
%------------------------------------------------------
    % classes in 1st column
    Class.A("DynamicStruct")
            ()
            ("+DynamicStruct()",
              "#copy()",
              "#setSize()");
    ClassTemplate.TA("Container")(A);
    Class.B("Accumulator")
            ()
            ("+Accumulator()",
             "+operator<<()",
             "+operator=()",
             "#bind()");
    ClassTemplate.TB("Container")(B);
    Class.C("HandleMap")
            ()
            ();
    ClassTemplate.TC("EigenType,", "Handle")(C);

    topToBottom(35)(A, B, C);
    drawObjects(A, TA, B, TB, C, TC);
%------------------------------------------------------

    % classes in 2nd column
    Class.D("DynamicStructContainer")
            ()
            ("+rootContainer()",
             "+storage()",
             "+byteStream()");
    D.nw = (120, 90);
    classStereotypes.D("<<concept>>");
    Class.E("Rebindable")
            ()
            ("+rebind()");
    classStereotypes.E("<<concept>>");
    Class.F("Ref")
            ()
            ("+operator T&()",
             "+ptr()");
    ClassTemplate.TF("T")(F);

    Note.EB("Member variables", "need to be", "Rebindable");
    EB.s = E.n + (40, 30);
    EB.info.foreColor := (.7, .7, .7);
    EB.info.iName.iFont.scale := 0.5;

    leftToRight(60)(B, E);
    topToBottom(30)(E, F);
    drawObjects(D, E, F, TF, EB);
%------------------------------------------------------

    % classes in 3rd column
    Class.G("DynamicStructRootContainer")
            ()();
    G.nw = (250, 30);
    ClassTemplate.TG("Storage")(G);
    Class.H("ByteStream")
            ()
            ("+seek()",
              "+rdstate()",
              "+size()",
              "+tell()",
              "+eof()",
              "+read<T>()");
    ClassTemplate.TH("StreamBuf")(H);
    Class.I("ByteStreamHandleBuf")
            ("#pos")
            ("+seek()",
              "+size()",
              "+tell()",
              "+resize()");
    ClassTemplate.TI("Storage")(I);
    Class.J("ContiguousDataHandle")
            ()
            ("+ptr()");
    classStereotypes.J("<<concept>>");

    topToBottom(35)(G, H, I, J);
    drawObjects(G, TG, H, TH, I, TI, J);
%------------------------------------------------------

    % links in 1st column
    link(inheritance)(pathManhattanY(A.n, D.w));
    link(inheritance)(B.n -- A.s);
    link(compositionUni)(E.w -- B.e);
    clink(dashedLink)(B, EB);
    link(inheritance)(C.n -- E.sw);

    % links in 2nd column
    link(association)(pathManhattanY(D.s, A.e));
    item(iAssoc)("1")(obj.nw = D.s);
    item(iAssoc)("container")(obj.ne = D.s);

    link(inheritance)(F.n -- E.s);

    % links in 3rd column
    link(inheritance)(pathManhattanY(G.n, D.e));

    link(association)(G.s -- H.n);
    item(iAssoc)("1")(obj.sw = H.n);
    item(iAssoc)("byteStream")(obj.se = H.n);

    link(association)(H.s -- I.n);
    item(iAssoc)("1")(obj.sw = I.n);
    item(iAssoc)("streamBuf")(obj.se = I.n);

    link(association)(I.s -- J.n);
    item(iAssoc)("1")(obj.sw = J.n);
    item(iAssoc)("storage")(obj.se = J.n);

endfig;
end
